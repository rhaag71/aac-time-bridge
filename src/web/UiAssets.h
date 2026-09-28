// Copyright (c) 2026 Rob Haag
// SPDX-License-Identifier: MIT
#pragma once
namespace aac {
// Constant, memory-mapped flash responses; streamed in bounded socket writes.
static const char kUiCss[] =
"HTTP/1.1 200 OK\r\nContent-Type: text/css; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"
"header .header-actions{display:flex;align-items:center;gap:20px}@media(max-width:720px){header .header-actions{gap:0}}"
R"CSS(:root{color-scheme:dark;--bg:#101513;--panel:#171e1b;--line:#34413a;--text:#e2e7df;--muted:#a2afa6;--accent:#bdd2be;--warn:#dfbb77;--red:#e6a49a;font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-size:15px;line-height:1.55}a{color:var(--accent)}button,input,summary{font:inherit}button,summary,a,input{-webkit-tap-highlight-color:transparent}button:focus-visible,summary:focus-visible,a:focus-visible,input:focus-visible{outline:2px solid var(--warn);outline-offset:4px}main{max-width:1160px;margin:auto;padding:28px 32px 36px}header{display:flex;justify-content:space-between;align-items:center;gap:20px;padding-bottom:26px;border-bottom:1px solid var(--line)}.brand{display:flex;align-items:center;gap:14px}.mark{display:grid;place-items:center;width:40px;height:40px;border:1px solid var(--accent);font:18px ui-monospace,monospace;color:var(--accent)}.brand strong{font-size:18px;letter-spacing:.05em}.eyebrow,.micro,dt,footer{font-family:ui-monospace,SFMono-Regular,Consolas,monospace}.eyebrow{font-size:11px;letter-spacing:.13em;text-transform:uppercase;color:var(--muted)}.micro{font-size:12px;color:var(--muted)}.top-meta{text-align:right}.intro{display:flex;justify-content:space-between;align-items:end;margin:30px 0 18px;gap:18px}h1{font-size:27px;font-weight:500;letter-spacing:-.04em;margin:3px 0}h2{font-size:17px;font-weight:550;margin:0}h3{font-size:15px;margin:0 0 8px}p{margin:8px 0 0}.muted{color:var(--muted)}.clock{border:1px solid var(--line);background:var(--panel);display:grid;grid-template-columns:minmax(0,1fr) 275px}.clock-main{padding:28px 30px 26px}.clock-top{display:flex;justify-content:space-between;align-items:center;gap:12px}.badge{display:inline-flex;align-items:center;gap:8px;font:11px ui-monospace,monospace;letter-spacing:.06em;color:var(--warn);white-space:nowrap}.badge:before{content:"";width:6px;height:6px;background:currentColor}.badge.valid{color:var(--accent)}.clock-readout{display:block;font:clamp(44px,7.8vw,96px)/1.2 ui-monospace,SFMono-Regular,Consolas,monospace;font-variant-numeric:tabular-nums;letter-spacing:-.055em;margin:24px 0 14px;color:var(--text)}.clock-note{color:var(--muted);font-size:13px;max-width:490px}.ruler{height:10px;border-top:1px solid var(--line);margin-top:29px;background:repeating-linear-gradient(90deg,var(--line) 0,var(--line) 1px,transparent 1px,transparent 16px);opacity:.7}.clock-side{border-left:1px solid var(--line);padding:28px 24px;display:flex;flex-direction:column;justify-content:space-between;gap:24px}.source{border-left:2px solid var(--warn);padding-left:14px}.source strong{display:block;font-size:18px;font-weight:500;margin:8px 0 2px}.source small{color:var(--muted)}.grid{display:grid;grid-template-columns:1fr 1fr;gap:24px;margin-top:24px}.section{border-top:1px solid var(--line);padding-top:18px}.section-heading{display:flex;align-items:center;justify-content:space-between;gap:14px;margin-bottom:16px}.section-heading .eyebrow{margin-bottom:5px}dl{margin:0}.row{display:grid;grid-template-columns:1fr minmax(0,1.3fr);gap:16px;padding:10px 0;border-bottom:1px solid #253029}dt{font-size:12px;color:var(--muted)}dd{margin:0;text-align:right;overflow-wrap:anywhere;font-size:13px}.network-value{text-transform:uppercase;color:var(--accent);letter-spacing:.04em}.notice{padding:12px 14px;border-left:2px solid var(--warn);background:#1d241e;color:var(--text);font-size:13px}.notice:empty{display:none}.setup{margin-top:26px;padding:22px 24px;background:var(--panel);border:1px solid var(--line)}.setup .section-heading{margin-bottom:8px}.setup p{color:var(--muted);font-size:13px}.form-grid{display:grid;grid-template-columns:1fr 1fr;gap:18px;margin:20px 0 16px}label{display:block;font-size:12px;color:var(--muted)}input{display:block;width:100%;min-height:44px;background:var(--bg);border:1px solid #526258;border-radius:0;color:var(--text);padding:10px 12px;margin-top:6px}input::placeholder{color:#78877d}.actions{display:flex;align-items:center;gap:14px;flex-wrap:wrap}button,.control{display:inline-block;border:1px solid #56695c;background:transparent;color:var(--text);padding:9px 16px;min-height:42px;border-radius:2px;text-decoration:none;cursor:pointer;font-size:13px}button:hover,.control:hover{background:#253129}button.primary{background:var(--accent);color:#142018;border-color:var(--accent);font-weight:600}button:disabled{opacity:.5;cursor:wait}.management{margin-top:30px}.management-grid{display:grid;grid-template-columns:1fr 1fr;gap:24px}.confirm{border:1px solid var(--line);background:var(--panel)}summary{list-style:none;cursor:pointer;padding:16px 18px;display:flex;justify-content:space-between;gap:12px;align-items:center;font-size:14px}summary::-webkit-details-marker{display:none}summary:after{content:"+";color:var(--muted);font-size:20px}details[open]>summary:after{content:"−"}summary small{font-size:12px;color:var(--muted);display:block}.confirm-body{padding:0 18px 18px;font-size:13px;color:var(--muted)}.confirm-body p{margin:0 0 16px}.danger{color:var(--red);border-color:#77534d}.confirm.danger summary{color:var(--red)}.danger button.danger:hover{background:#352522}.feedback{margin-top:16px}.footnote{font-size:12px;color:var(--muted);margin-top:16px}footer{display:flex;justify-content:space-between;gap:14px;border-top:1px solid var(--line);padding-top:18px;margin-top:30px;font-size:10px;letter-spacing:.06em;color:var(--muted)}[hidden]{display:none!important}@media(max-width:720px){main{padding:20px 18px 26px}.top-meta{display:none}.intro{margin-top:24px}h1{font-size:24px}.clock{grid-template-columns:1fr}.clock-main{padding:20px}.clock-top{align-items:start;flex-direction:column;gap:8px}.clock-readout{font-size:clamp(48px,14vw,78px);margin:20px 0 12px}.clock-side{border-left:0;border-top:1px solid var(--line);padding:18px 20px;display:grid;grid-template-columns:1fr 1fr;gap:12px}.grid,.management-grid,.form-grid{grid-template-columns:1fr;gap:22px}.setup{padding:20px}.brand strong{font-size:16px}.row{grid-template-columns:1fr 1.2fr}footer{flex-direction:column;gap:3px}.actions button{flex-grow:1}})CSS";
static const char kUiJs[] =
"HTTP/1.1 200 OK\r\nContent-Type: text/javascript; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"
R"JS((()=>{
'use strict';
let actionBusy=false,statusBusy=false,timer=0;
const notice=document.getElementById('feedback'),stamp=document.getElementById('snapshot-note');
function say(text){notice.textContent=text;notice.hidden=false}
function lock(value){document.querySelectorAll('button').forEach(b=>b.disabled=value)}
function schedule(delay){clearTimeout(timer);timer=setTimeout(poll,delay)}
async function getTelemetry(){
 const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),1800);
 try{const response=await fetch('/telemetry',{cache:'no-store',signal:controller.signal});if(!response.ok)throw Error('status');return await response.json()}
 finally{clearTimeout(timeout)}
}
async function postForm(form,body){
 const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),5000);
 try{return await fetch(form.getAttribute('action'),{method:'POST',cache:'no-store',headers:{'Content-Type':'application/x-www-form-urlencoded'},body,signal:controller.signal})}
 finally{clearTimeout(timeout)}
}
function put(id,value){const node=document.getElementById(id);if(node)node.textContent=value}
function invalidate(text){
 put('clock-readout','--:--:--');put('clock-status','APPLIANCE UNREACHABLE');
 document.getElementById('clock-status').classList.remove('valid');put('clock-note',text);
 if(stamp)stamp.textContent='Live telemetry unavailable; authoritative UTC cleared.'
}
function apply(d){
 const valid=d.utcValid===true&&Number.isSafeInteger(d.utcSeconds);
 if(valid){
  const day=((d.utcSeconds%86400)+86400)%86400;
  put('clock-readout',String(Math.floor(day/3600)).padStart(2,'0')+':'+String(Math.floor(day/60)%60).padStart(2,'0')+':'+String(day%60).padStart(2,'0'));
  put('clock-status','SYNCHRONIZED');document.getElementById('clock-status').classList.add('valid');
 }else{
  put('clock-readout','--:--:--');put('clock-status','UNSYNCHRONIZED');document.getElementById('clock-status').classList.remove('valid');
 }
 put('clock-note',d.clockNote);put('source-availability',d.sourceAvailability);put('source-error',d.sourceError);
 put('selected-authority',d.selectedAuthority);put('network-state',d.networkState);put('network-state-heading',d.networkState);
 put('lan-address',d.lanAddress);put('recovery-ap',d.recoveryAp);put('ap-address',d.apAddress);
 put('configuration-storage',d.configurationStorage);put('ap-operation',d.apOperation);
 put('watchdog-status',d.watchdogStatus);put('reset-reason',d.resetReason);put('watchdog-reset',d.watchdogReset);
 put('ntp-service',d.ntpService==='listening'?'Listening on UDP/123':d.ntpService==='socket-error'?'Socket error ('+d.ntpLastError+')':'Not listening (off-LAN)');
 put('ntp-state',d.ntpState);put('ntp-reference',d.ntpReference);
 put('ntp-requests',d.ntpRequests+' requests / '+d.ntpReplies+' replies ('+d.ntpSynchronizedReplies+' sync, '+d.ntpUnsynchronizedReplies+' unsync, '+d.ntpRejectedRequests+' rejected)');
 put('uptime-snapshot',d.uptimeSeconds+' s');put('source-utc-quality',d.sourceUtcQuality);
 put('pico-packet',d.picoPacket);put('pico-sequences',d.picoSequences);put('pico-flags',d.picoFlags);
 put('pico-satellites',d.picoSatellites);put('packet-age',d.packetAgeMs===null?'Unavailable':d.packetAgeMs+' ms');
 put('edge-age',d.edgeAgeMs===null?'Unavailable':d.edgeAgeMs+' ms');put('phase-association',d.phaseAssociation);
 put('source-qualification',d.sourceQualification);put('pico-acquisition',d.picoAcquisition);
 put('setup-feedback',d.setupMessage);put('setup-ap-name',d.apName);
 if(stamp)stamp.textContent='Live appliance telemetry · latest update from the bridge · UTC is never browser-generated.';
}
async function poll(){
 if(statusBusy||actionBusy){schedule(250);return}
 statusBusy=true;
 try{apply(await getTelemetry())}
 catch(_){invalidate('No recent appliance telemetry. Authoritative UTC is not being displayed.')}
 finally{statusBusy=false;schedule(1000)}
}
document.getElementById('refresh').addEventListener('click',()=>{clearTimeout(timer);poll()});
document.addEventListener('visibilitychange',()=>{if(!document.hidden){clearTimeout(timer);poll()}});
document.querySelectorAll('[data-cancel]').forEach(button=>button.addEventListener('click',()=>button.closest('details').open=false));
document.querySelectorAll('form[data-action]').forEach(form=>form.addEventListener('submit',async event=>{
 event.preventDefault();if(actionBusy)return;
 const action=form.dataset.action,data=new URLSearchParams(new FormData(form));
 if(event.submitter&&event.submitter.name)data.set(event.submitter.name,event.submitter.value);
 else if(action!=='wifi'){say('Use the explicit confirmation button to continue.');return}
 const password=form.querySelector('[name=password]');if(password)password.value='';
 actionBusy=true;lock(true);say(action==='wifi'?'Saving configuration…':'Sending confirmed request…');
 try{
  const response=await postForm(form,data.toString()),message=await response.text();say(message);
  if(response.ok){form.closest('details')?.removeAttribute('open');if(action!=='wifi')invalidate('Restart requested. Reopen this page after the appliance returns.')}
 }catch(_){say('Request delivery is uncertain; it was not retried. After factory reset, connect to the open AAC-Bridge-XXXXXX AP at 192.168.4.1.')}
 finally{actionBusy=false;lock(false);schedule(0)}
}));
poll();
})();)JS";
}
