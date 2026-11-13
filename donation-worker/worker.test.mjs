import test from 'node:test';
import assert from 'node:assert/strict';
import worker,{parseAmount} from './worker.mjs';
const env={SITE_ORIGIN:'http://127.0.0.1:8787',RETURN_URL:'http://127.0.0.1:8787/support.html',STRIPE_SECRET_KEY:'sk_test_fake',RATE_LIMITER:{limit:async()=>({success:true})}};
const call=(path,body,overrides={})=>worker.fetch(new Request(env.SITE_ORIGIN+path,{method:'POST',headers:{Origin:env.SITE_ORIGIN,'Content-Type':'application/json'},body:JSON.stringify(body)}),{...env,...overrides});
const valid={amount:'20.01',nickname:'<test>',requestId:'12345678-1234-1234-1234-123456789abc'};
test('exact cents and bounds',()=>{assert.equal(parseAmount('20.01'),2001);assert.equal(parseAmount('10'),1000);assert.equal(parseAmount('2000'),200000);for(const v of ['9.99','2000.01','1e2','20.001','-10','NaN','',20,null])assert.throws(()=>parseAmount(v));});
test('live keys and missing limiter fail closed',async()=>{assert.equal((await call('/api/checkout',valid,{STRIPE_SECRET_KEY:'sk_live_fake'})).status,503);assert.equal((await call('/api/checkout',valid,{RATE_LIMITER:null})).status,503);assert.equal((await call('/api/checkout',valid,{RATE_LIMITER:{limit:async()=>({success:false})}})).status,429);});
test('invalid amount and nickname rejected',async()=>{assert.equal((await call('/api/checkout',{...valid,amount:'0'})).status,400);assert.equal((await call('/api/checkout',{...valid,nickname:'a'.repeat(61)})).status,400);});
test('foreign origin rejected',async()=>{const r=await worker.fetch(new Request('http://localhost/api/checkout',{method:'POST',headers:{Origin:'https://evil.example'}}),env);assert.equal(r.status,403);});
test('checkout uses exact amount, trusted return URL, idempotency',async()=>{
 const saved=globalThis.fetch;
 globalThis.fetch=async(url,options)=>{assert.equal(url,'https://api.stripe.com/v1/checkout/sessions');assert.equal(options.body.get('line_items[0][price_data][unit_amount]'),'2001');assert.equal(options.body.get('success_url'),env.RETURN_URL+'?session_id={CHECKOUT_SESSION_ID}');assert.equal(options.headers['Idempotency-Key'],'vts-support-'+valid.requestId);return Response.json({livemode:false,url:'https://checkout.stripe.com/test'});};
 try{const r=await call('/api/checkout',{...valid,return_url:'https://evil.example'});assert.equal(r.status,200);}finally{globalThis.fetch=saved;}
});
test('result is verified and no personal information returned',async()=>{
 const saved=globalThis.fetch;
 try{for(const [payment_status,status,paid] of [['paid','complete',true],['unpaid','complete',false],['paid','open',false]]){globalThis.fetch=async()=>Response.json({livemode:false,metadata:{project:'vtsfloat-support-test'},payment_status,status,customer_email:'private@example.com'});const r=await call('/api/status',{sessionId:'cs_test_123'});assert.deepEqual(await r.json(),{paid});}
 globalThis.fetch=async()=>Response.json({livemode:false,metadata:{project:'other'}});assert.equal((await call('/api/status',{sessionId:'cs_test_123'})).status,404);
 }finally{globalThis.fetch=saved;}
});
