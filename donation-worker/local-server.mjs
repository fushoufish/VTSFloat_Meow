import http from 'node:http';
import {readFile} from 'node:fs/promises';
import {resolve,extname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {randomUUID} from 'node:crypto';
import worker,{parseAmount} from './worker.mjs';
const root=fileURLToPath(new URL('../docs/',import.meta.url));
const port=Number(process.env.PORT||8787),origin=`http://127.0.0.1:${port}`;
const mock=!process.env.STRIPE_SECRET_KEY;
if(!mock&&!process.env.STRIPE_SECRET_KEY.startsWith('sk_test_'))throw Error('Only sk_test_ keys are allowed.');
const sessions=new Map(),requests=new Map();
let count=0,reset=Date.now()+60000;
const env={RETURN_URLS:{[origin]:origin+'/support.html'},STRIPE_SECRET_KEY:process.env.STRIPE_SECRET_KEY,RATE_LIMITER:{async limit(){if(Date.now()>reset){count=0;reset=Date.now()+60000;}return{success:++count<=30};}}};
const types={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css','.webp':'image/webp','.svg':'image/svg+xml','.png':'image/png','.json':'application/json'};
http.createServer(async(req,res)=>{
 const send=(status,body,type='application/json')=>{res.writeHead(status,{'Content-Type':type,'Cache-Control':'no-store'});res.end(typeof body==='string'||Buffer.isBuffer(body)?body:JSON.stringify(body));};
 try{
  if(req.headers.host!==`127.0.0.1:${port}`)return send(403,{error:'Use the 127.0.0.1 preview address.'});
  const url=new URL(req.url,origin);
  if(url.pathname==='/api/config')return send(200,{mock,currency:'cny'});
  if(url.pathname==='/mock-checkout'&&mock){
   const id=url.searchParams.get('id'),session=sessions.get(id);if(!session)return send(404,{error:'Demo session expired.'});
   return send(200,`<!doctype html><html lang="en"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Local demo checkout</title><body style="font:18px/1.7 system-ui;max-width:550px;margin:60px auto;padding:24px"><h1>Local demo checkout</h1><p>This is a simulation, not Stripe. No card details or money are collected.</p><p>Amount: ${session.currency.toUpperCase()} ${(session.cents/100).toFixed(2)}</p><form method="post" action="/mock-complete"><input type="hidden" name="id" value="${id}"><button style="padding:14px">Simulate successful payment</button></form><p><a href="/support.html?status=cancel">Cancel and return</a></p></body></html>`,'text/html; charset=utf-8');
  }
  if(url.pathname.startsWith('/api/')||url.pathname==='/mock-complete'){
   if(req.method!=='POST')return send(405,{error:'POST required.'});
   if(req.headers.origin!==origin)return send(403,{error:'Origin not allowed.'});
   let raw='';for await(const chunk of req){raw+=chunk;if(raw.length>2048)return send(413,{error:'Request too large.'});}
   if(url.pathname==='/mock-complete'&&mock){const id=new URLSearchParams(raw).get('id');if(!sessions.has(id))return send(404,{error:'Session not found.'});sessions.get(id).paid=true;res.writeHead(303,{Location:'/support.html?session_id='+id});return res.end();}
   if(!mock){const response=await worker.fetch(new Request(url,{method:'POST',headers:req.headers,body:raw}),env);res.writeHead(response.status,Object.fromEntries(response.headers));return res.end(await response.text());}
   if(!(await env.RATE_LIMITER.limit()).success)return send(429,{error:'Too many requests. Wait a minute.'});
   if(!req.headers['content-type']?.startsWith('application/json'))return send(415,{error:'JSON required.'});
   let data;try{data=JSON.parse(raw);if(!data||typeof data!=='object')throw Error();}catch{return send(400,{error:'Invalid JSON.'});}
   if(url.pathname==='/api/checkout'){
    const currency=data.currency===undefined?'cny':data.currency;
    let cents;try{cents=parseAmount(data.amount,currency);}catch(e){return send(400,{error:e.message});}
    if(typeof data.nickname!=='string'||data.nickname.length>60||!/^[a-f0-9-]{36}$/.test(data.requestId||''))return send(400,{error:'Invalid nickname or request ID.'});
    const prior=requests.get(data.requestId),fingerprint=JSON.stringify([currency,cents,data.nickname]);
    if(prior&&prior.fingerprint!==fingerprint)return send(409,{error:'Request changed. Please edit the form and retry.'});
    const id=prior?.id||'demo_'+randomUUID();
    if(!prior){sessions.set(id,{currency,cents,paid:false});requests.set(data.requestId,{id,fingerprint});}
    return send(200,{url:origin+'/mock-checkout?id='+id});
   }
   if(url.pathname==='/api/status'){if(String(data.sessionId).startsWith('cs_test_'))return send(409,{error:'Returned from Stripe test checkout. Connect a Stripe test key to verify payment automatically.'});const session=sessions.get(data.sessionId);return session?send(200,{paid:session.paid}):send(404,{error:'Test session not found. It may have expired.'});}
   return send(404,{error:'Not found.'});
  }
  const path=resolve(root,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
  if(!path.startsWith(resolve(root)+'\\')&&!path.startsWith(resolve(root)+'/'))return send(403,{error:'Forbidden.'});
  try{return send(200,await readFile(path),types[extname(path)]||'application/octet-stream');}catch{return send(404,{error:'Not found.'});}
 }catch{send(500,{error:'Local preview error.'});}
}).listen(port,'127.0.0.1',()=>console.log(`Preview: ${origin}/support.html (${mock?'local simulation':'Stripe test mode'})`));
