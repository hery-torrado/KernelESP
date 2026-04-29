let liveRun=0;
function sleep(ms){return new Promise(r=>setTimeout(r,ms))}
async function apiJsonTimeout(p,ms=4500){let c=new AbortController(),t=setTimeout(()=>c.abort(),ms);try{let r=await fetch(apiUrl(p),{cache:"no-store",credentials:"same-origin",signal:c.signal});if(!r.ok)throw Error("HTTP "+r.status);return await r.json()}finally{clearTimeout(t)}}
async function apiCmdLive(c){try{let d=await apiJsonTimeout("/api/cmd?c="+encodeURIComponent(c));return d.output||"(no output)"}catch(e){return"timeout or error: "+e.message}}
function refreshLive(){let a=$(".panel.active");if(!a)return Promise.resolve();let run=++liveRun,jobs=[];$$("[data-live]",a).forEach(e=>{let c=e.dataset.live;e.textContent="loading "+c+"...";jobs.push(async()=>{let t=await apiCmdLive(c);if(run==liveRun&&document.contains(e))e.textContent=t})});let s=$("#sensor");if(s)jobs.push(async()=>{s.textContent="loading sensor...";try{let j=await apiJsonTimeout("/api/sensor");if(run==liveRun&&document.contains(s))s.textContent=j.ok?JSON.stringify(j,null,2):"sensor: not found"}catch(e){if(run==liveRun&&document.contains(s))s.textContent="sensor: timeout or error "+e.message}});(async()=>{for(let j of jobs){if(run!=liveRun)return;await j();await sleep(40)}})();return Promise.resolve()}
async function refreshAll(live=true){try{await refreshStatus();if(live)refreshLive()}catch(e){$("#summary").textContent="offline or unauthorized: "+e.message}}
function keepKey(){if(!key)return;$$("nav a").forEach(a=>{let u=new URL(a.href,location.href);u.searchParams.set("key",key);a.href=u.pathname+u.search})}
function validateClockFields(f){syncClockFields(f);for(let e of $$("[data-clock]",f)){let v=String(e.value||"").trim();e.value=v;e.setCustomValidity(clockOk(v)?"":"Use 24-hour time as HH:MM, for example 08:00 or 23:30.");if(!clockOk(v)){e.reportValidity();return false}}return true}
window.kespClockOk=clockOk;window.kespValidateClockFields=validateClockFields;
function fill(t,f){syncClockFields(f);new FormData(f).forEach((v,n)=>t=t.split("{"+n+"}").join(String(v).trim()));return t}
async function relayAction(n,s){let ms=Number($("#pulseMs")?.value||500),r=await fetch(apiUrl(`/api/relay?name=${encodeURIComponent(n)}&state=${encodeURIComponent(s)}&ms=${Math.max(50,Math.min(60000,ms))}`),{cache:"no-store",credentials:"same-origin"});if(!r.ok)throw Error("relay HTTP "+r.status);await refreshAll(false)}
function makeOut(c){let o=$(".formOut",c);if(!o){c.insertAdjacentHTML("beforeend",`<pre class="formOut"></pre>`);o=$(".formOut",c);if(window.kespEnhancePre)window.kespEnhancePre()}return o}
function outFor(x){let c=x.closest(".card");return c?makeOut(c):$(".panel.active .formOut")||$("#cmdOut")}
window.kespBoot=function(){keepKey();setupUi();setupMore();refreshAll(true);setTimeout(()=>refreshAll(false),2500);setInterval(()=>refreshAll(true),60000)}
window.kespReady=1;
if(window.kespStart)window.kespStart();
