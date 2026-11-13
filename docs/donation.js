(async()=>{
 const form=document.querySelector('#support-form');
 if(!form)return;
 const status=document.querySelector('#status'),field=form.querySelector('fieldset'),amount=document.querySelector('#amount');
 const translate = text => window.VTSFloatI18n?.translate(text) ?? text;
 const show=text=>{status.hidden=false;status.textContent=translate(text);};
 const local=['localhost','127.0.0.1'].includes(location.hostname);
 const apiBase=local?'':'https://vtsfloat-meow-support.donation-worker.workers.dev';
 const api=async(path,body)=>{
  const r=await fetch(apiBase+path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body),signal:AbortSignal.timeout(20000)});
  const data=await r.json();if(!r.ok)throw Error(local ? data.error || 'Request failed.' : 'Payment service unavailable. Please try again.');return data;
 };
 let requestId=crypto.randomUUID(),busy=false,config;
 try {
  const r=await fetch(apiBase+'/api/config',{signal:AbortSignal.timeout(10000)});if(!r.ok)throw Error('Payment service unavailable. Please try again.');
  config=await r.json();
  if((!local&&!config.live)||(config.live&&!config.ready))throw Error('Payments are not open yet. Thank you for your support!');
  document.querySelector('#mode').textContent=translate(config.live?(local?'STRIPE LIVE MODE · REAL CNY 10 PAYMENT':'ONE-TIME SUPPORT'):config.mock?'LOCAL DEMO · NO MONEY MOVES':'STRIPE TEST MODE · NO REAL PAYMENTS');
  document.querySelector('#pay').textContent=translate(config.live?'Continue to Stripe live checkout →':config.mock?'Continue to demo checkout →':'Continue to Stripe test checkout →');
  if(config.live&&local){amount.value='10';amount.min='10';amount.max='10';amount.readOnly=true;document.querySelector('#amount-hint').textContent='CNY 10 · One-time support';document.querySelector('.foot').innerHTML='Support is optional. All features remain free.<br>This is a real payment through Stripe.';document.querySelectorAll('[data-amount]').forEach(b=>{b.disabled=true;b.setAttribute('aria-pressed',String(b.dataset.amount==='10'));});}
  if(config.live&&!local)document.querySelector('.foot').textContent=translate('Support is optional. All features remain free.');
  field.disabled=false;status.hidden=true;
 }catch(e){show(local || e.message === 'Payments are not open yet. Thank you for your support!' ? e.message : 'Payment service unavailable. Please try again.');return;}
 document.querySelectorAll('[data-amount]').forEach(b=>b.addEventListener('click',()=>{amount.value=b.dataset.amount;amount.dispatchEvent(new Event('input',{bubbles:true}));}));
 form.addEventListener('input',()=>{requestId=crypto.randomUUID();document.querySelectorAll('[data-amount]').forEach(b=>b.setAttribute('aria-pressed',String(Number(b.dataset.amount)===Number(amount.value))));});
 const params=new URLSearchParams(location.search);
 if(params.get('status')==='cancel')show('Checkout cancelled. No payment was confirmed.');
 if(params.has('session_id')){
  show('Checking payment result…');
  try{const result=await api('/api/status',{sessionId:params.get('session_id')});show(result.paid?(config.live?'Payment confirmed. Thank you for your support!':'Test payment confirmed. Thank you! No real money was charged.'):'Payment is not confirmed yet. Check again later.');}catch(e){show(e.message);}
 }
 form.addEventListener('submit',async e=>{
  e.preventDefault();if(busy||!form.reportValidity())return;
  busy=true;field.disabled=true;show(config.live?'Creating your live checkout…':'Creating your test checkout…');
  try{
   const data=await api('/api/checkout',{amount:amount.value,nickname:document.querySelector('#nickname').value,requestId});
   const url=new URL(data.url);
   if(!(url.protocol==='https:'&&url.hostname==='checkout.stripe.com')&&!(url.origin===location.origin&&url.pathname==='/mock-checkout'))throw Error('Unexpected checkout address.');
   location.assign(url.href);
  }catch(e){show(e.name==='TimeoutError'?'Request timed out. Please retry.':local?e.message:'Payment service unavailable. Please try again.');busy=false;field.disabled=false;}
 });
})();
