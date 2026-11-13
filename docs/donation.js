(async()=>{
 const form=document.querySelector('#support-form');
 if(!form)return;
 const status=document.querySelector('#status'),field=form.querySelector('fieldset'),amount=document.querySelector('#amount');
 const mode=document.querySelector('#mode'),paymentNote=document.querySelector('#payment-note');
 const translate = text => window.VTSFloatI18n?.translate(text) ?? text;
 const show=text=>{status.hidden=false;status.textContent=translate(text);};
 const local=['localhost','127.0.0.1'].includes(location.hostname);
 const apiBase=local?'':'https://vtsfloat-meow-support.donation-worker.workers.dev';
 const unavailable='Payment service unavailable. Please try again.';
 const closed='Donations aren’t open yet. Thanks for stopping by!';
 const timeout='Request timed out. Please retry.';
 const api=async(path,body)=>{
  const r=await fetch(apiBase+path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body),signal:AbortSignal.timeout(20000)});
  const data=await r.json();if(!r.ok)throw Error(local ? data.error || unavailable : unavailable);return data;
 };
 const showError=e=>{
  if(e.name==='TimeoutError'){show(timeout);return;}
  const message=e.message;
  show(message===closed || (local && translate(message)!==message) || (local && window.VTSFloatI18n?.getLocale()==='en' && e.name==='Error') ? message : unavailable);
 };
 let requestId=crypto.randomUUID(),busy=false,config;
 try {
  const r=await fetch(apiBase+'/api/config',{signal:AbortSignal.timeout(10000)});if(!r.ok)throw Error(unavailable);
  config=await r.json();
  if(local ? config.live : !config.live||!config.ready)throw Error(closed);
  if(config.currency!=='cny')throw Error(unavailable);
  mode.textContent=translate(config.live?'One-time donation':config.mock?'Local demo':'Stripe test mode');
  mode.hidden=false;
  if(!config.live){
   paymentNote.textContent=translate('You won’t be charged in this preview.');
   paymentNote.hidden=false;
  }
  field.disabled=false;status.hidden=true;
 }catch(e){showError(e);return;}
 document.querySelectorAll('[data-amount]').forEach(b=>b.addEventListener('click',()=>{amount.value=b.dataset.amount;amount.dispatchEvent(new Event('input',{bubbles:true}));}));
 form.addEventListener('input',()=>{requestId=crypto.randomUUID();document.querySelectorAll('[data-amount]').forEach(b=>b.setAttribute('aria-pressed',String(Number(b.dataset.amount)===Number(amount.value))));});
 const params=new URLSearchParams(location.search);
 if(params.get('status')==='cancel')show('Checkout cancelled. No payment was confirmed.');
 if(params.has('session_id')){
  show('Checking payment result…');
  try{const result=await api('/api/status',{sessionId:params.get('session_id')});show(result.paid?(config.live?'Payment confirmed. Thank you for your support!':'Test payment confirmed. You weren’t charged.'):'Payment is not confirmed yet. Check again later.');}catch(e){showError(e);}
 }
 form.addEventListener('submit',async e=>{
  e.preventDefault();if(busy||!form.reportValidity())return;
  busy=true;field.disabled=true;show(config.live?'Creating your live checkout…':config.mock?'Opening demo checkout…':'Opening Stripe test checkout…');
  try{
   const data=await api('/api/checkout',{currency:'cny',amount:amount.value,nickname:document.querySelector('#nickname').value,requestId});
   const url=new URL(data.url);
   if(!(url.protocol==='https:'&&url.hostname==='checkout.stripe.com')&&!(url.origin===location.origin&&url.pathname==='/mock-checkout'))throw Error('Unexpected checkout address.');
   location.assign(url.href);
  }catch(e){showError(e);busy=false;field.disabled=false;}
 });
})();
