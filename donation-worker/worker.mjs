export function parseAmount(value) {
  if (typeof value !== 'string' || !/^\d{1,4}(\.\d{1,2})?$/.test(value)) throw Error('Use at most two decimal places.');
  const [whole, fraction = ''] = value.split('.');
  const cents = Number(whole)*100 + Number(fraction.padEnd(2,'0'));
  if (cents < 1000 || cents > 200000) throw Error('Choose CNY 10 to CNY 2,000.');
  return cents;
}
export default {
 async fetch(request,env) {
  const headers = {'Content-Type':'application/json','Cache-Control':'no-store'};
  const reply = (data,status=200) => new Response(JSON.stringify(data),{status,headers});
  if(request.headers.get('Origin') !== env.SITE_ORIGIN) return reply({error:'Origin not allowed.'},403);
  headers['Access-Control-Allow-Origin']=env.SITE_ORIGIN;
  headers.Vary='Origin';
  const live = env.STRIPE_MODE === 'live';
  const keyPattern = live ? /^(rk|sk)_live_/ : /^(rk|sk)_test_/;
  const ready = keyPattern.test(env.STRIPE_SECRET_KEY || '') && !!env.RATE_LIMITER;
  const project = live ? 'vtsfloat-support' : 'vtsfloat-support-test';
  const path = new URL(request.url).pathname;
  if(request.method==='GET' && path==='/api/config') return reply({live,ready});
  if(request.method==='OPTIONS') return new Response(null,{status:204,headers:{...headers,'Access-Control-Allow-Methods':'POST','Access-Control-Allow-Headers':'Content-Type'}});
  if(request.method!=='POST') return reply({error:'Method not allowed.'},405);
  if(!request.headers.get('Content-Type')?.startsWith('application/json')) return reply({error:'JSON required.'},415);
  if(!keyPattern.test(env.STRIPE_SECRET_KEY || '')) return reply({error:live?'Payments are not open yet.':'A Stripe test secret key is required.'},503);
  if(!env.RATE_LIMITER) return reply({error:'Rate limiter is missing.'},503);
  if(!(await env.RATE_LIMITER.limit({key:request.headers.get('CF-Connecting-IP')||'local'})).success) return reply({error:'Too many requests. Wait a minute.'},429);
  const raw=await request.text();
  if(raw.length>2048) return reply({error:'Request too large.'},413);
  let data;
  try { data=JSON.parse(raw); if(!data || typeof data!=='object' || Array.isArray(data)) throw Error(); }
  catch { return reply({error:'Invalid JSON.'},400); }
  const stripe=async(suffix,options={})=>{
   const response=await fetch('https://api.stripe.com/v1/checkout/sessions'+suffix,{...options,headers:{Authorization:`Bearer ${env.STRIPE_SECRET_KEY}`,...options.headers},signal:AbortSignal.timeout(15000)});
   const result=await response.json();
   if(!response.ok) throw Error('Stripe rejected the payment request. Please try again later.');
   return result;
  };
  try {
   if(path==='/api/checkout') {
    let cents;
    try { cents=parseAmount(data.amount); } catch(e) { return reply({error:e.message},400); }
    if(typeof data.nickname!=='string' || data.nickname.length>60 || !/^[a-f0-9-]{36}$/.test(data.requestId||'')) return reply({error:'Invalid nickname or request ID.'},400);
    const body=new URLSearchParams({mode:'payment','managed_payments[enabled]':'false','line_items[0][price_data][currency]':'cny','line_items[0][price_data][unit_amount]':String(cents),'line_items[0][price_data][product_data][name]':live?'Support VTSFloat_Meow':'Support VTSFloat_Meow (test)','line_items[0][quantity]':'1','metadata[project]':project,'metadata[nickname]':data.nickname.trim(),success_url:env.RETURN_URL+'?session_id={CHECKOUT_SESSION_ID}',cancel_url:env.RETURN_URL+'?status=cancel'});
    const result=await stripe('',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','Idempotency-Key':'vts-support-'+data.requestId},body});
    if(result.livemode!==live || !result.url?.startsWith('https://checkout.stripe.com/')) throw Error('Unexpected checkout response.');
    return reply({url:result.url});
   }
   if(path==='/api/status') {
    const sessionPattern = live ? /^cs_live_[A-Za-z0-9]{1,200}$/ : /^cs_test_[A-Za-z0-9]{1,200}$/;
    if(!sessionPattern.test(data.sessionId||'')) return reply({error:'Invalid payment session.'},400);
    const result=await stripe('/'+encodeURIComponent(data.sessionId));
    if(result.livemode!==live || result.metadata?.project!==project) return reply({error:'Session not found.'},404);
    return reply({paid:result.status==='complete' && result.payment_status==='paid'});
   }
   return reply({error:'Not found.'},404);
  } catch(e) { return reply({error:e.name==='TimeoutError'?'Stripe timed out. Please retry.':e.message},502); }
 }
};
