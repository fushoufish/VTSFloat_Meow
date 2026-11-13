import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import worker,{parseAmount} from './worker.mjs';
const {vars:{RETURN_URLS}}=JSON.parse(readFileSync(new URL('./wrangler.jsonc',import.meta.url),'utf8'));
const sites=Object.entries(RETURN_URLS);
const env={RETURN_URLS,STRIPE_SECRET_KEY:'sk_test_fake',RATE_LIMITER:{limit:async()=>({success:true})}};
const workerOrigin='https://vtsfloat-meow-support.donation-worker.workers.dev';
const call=(path,body,overrides={},origin=sites[0][0])=>worker.fetch(new Request(workerOrigin+path,{method:'POST',headers:{Origin:origin,'Content-Type':'application/json'},body:JSON.stringify(body)}),{...env,...overrides});
const valid={currency:'usd',amount:'1.00',nickname:'<test>',requestId:'12345678-1234-1234-1234-123456789abc'};
test('exact USD cents and bounds',()=>{assert.equal(parseAmount('20.01'),2001);assert.equal(parseAmount('1'),100);assert.equal(parseAmount('300'),30000);assert.equal(parseAmount('10','cny'),1000);for(const v of ['0.99','300.01','1e2','20.001','-10','NaN','',20,null])assert.throws(()=>parseAmount(v));assert.throws(()=>parseAmount('1','eur'));});
test('live keys and missing limiter fail closed',async()=>{assert.equal((await call('/api/checkout',valid,{STRIPE_SECRET_KEY:'sk_live_fake'})).status,503);assert.equal((await call('/api/checkout',valid,{RATE_LIMITER:null})).status,503);assert.equal((await call('/api/checkout',valid,{RATE_LIMITER:{limit:async()=>({success:false})}})).status,429);});
test('invalid amount and nickname rejected',async()=>{assert.equal((await call('/api/checkout',{...valid,amount:'0'})).status,400);assert.equal((await call('/api/checkout',{...valid,nickname:'a'.repeat(61)})).status,400);});
test('both production origins pass CORS; unlisted and missing origins are rejected',async()=>{
 for(const [origin] of sites){
  const config=await worker.fetch(new Request(workerOrigin+'/api/config',{headers:{Origin:origin}}),env);
  assert.equal(config.headers.get('access-control-allow-origin'),origin);
  assert.deepEqual(await config.json(),{live:false,ready:true,currency:'usd'});
  const preflight=await worker.fetch(new Request(workerOrigin+'/api/checkout',{method:'OPTIONS',headers:{Origin:origin,'Access-Control-Request-Method':'POST'}}),env);
  assert.equal(preflight.status,204);
  assert.equal(preflight.headers.get('access-control-allow-origin'),origin);
 }
 for(const origin of ['https://evil.example','constructor',null]){
  const r=await worker.fetch(new Request(workerOrigin+'/api/config',{headers:origin?{Origin:origin}:{}}),env);
  assert.equal(r.status,403);
  assert.equal(r.headers.get('access-control-allow-origin'),null);
 }
});
test('both sites create a USD checkout and return to their own page; cached pages stay in CNY',async()=>{
 const saved=globalThis.fetch;
 try{
  for(const [origin,returnUrl] of sites) for(const currency of ['usd','cny']){
   globalThis.fetch=async(url,options)=>{
    assert.equal(url,'https://api.stripe.com/v1/checkout/sessions');
    assert.equal(options.body.get('line_items[0][price_data][currency]'),currency);
    assert.equal(options.body.get('line_items[0][price_data][unit_amount]'),currency==='usd'?'100':'2001');
    assert.equal(options.body.get('adaptive_pricing[enabled]'),currency==='usd'?'false':null);
    assert.equal(options.body.get('success_url'),returnUrl+'?session_id={CHECKOUT_SESSION_ID}');
    assert.equal(options.body.get('cancel_url'),returnUrl+'?status=cancel');
    assert.equal(options.headers['Idempotency-Key'],'vts-support-'+valid.requestId);
    return Response.json({livemode:false,url:'https://checkout.stripe.com/test'});
   };
   const r=await call('/api/checkout',{...valid,currency:currency==='usd'?currency:undefined,amount:currency==='usd'?'1.00':'20.01',return_url:'https://evil.example'},{},origin);
   assert.equal(r.status,200,await r.clone().text());
   assert.equal(r.headers.get('access-control-allow-origin'),origin);
  }
 }finally{globalThis.fetch=saved;}
});
test('both sites verify payment status without returning personal information',async()=>{
 const saved=globalThis.fetch;
 try{for(const [origin] of sites) for(const [payment_status,status,paid] of [['paid','complete',true],['unpaid','complete',false],['paid','open',false]]){globalThis.fetch=async()=>Response.json({livemode:false,metadata:{project:'vtsfloat-support-test'},payment_status,status,customer_email:'private@example.com'});const r=await call('/api/status',{sessionId:'cs_test_123'},{},origin);assert.deepEqual(await r.json(),{paid});}
 globalThis.fetch=async()=>Response.json({livemode:false,metadata:{project:'other'}});assert.equal((await call('/api/status',{sessionId:'cs_test_123'})).status,404);
 }finally{globalThis.fetch=saved;}
});
