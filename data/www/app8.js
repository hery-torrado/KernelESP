(function(){
const REC="kesp.scripts";
function scriptRecs(){try{return JSON.parse(localStorage.getItem(REC)||"[]")}catch(e){return[]}}
function saveRec(p){p=String(p||"").trim();if(!p)return;let r=scriptRecs().filter(x=>x!==p);r.unshift(p);localStorage.setItem(REC,JSON.stringify(r.slice(0,10)));drawRecs()}
function pth(){return ($("#scriptPath")?.value||"/home/test.sh").trim()}
function setScriptOut(t){let o=$("#scriptOut");if(o)o.textContent=t}
function lineNums(){let t=$("#scriptText"),n=$("#scriptLines");if(!t||!n)return;let c=(t.value.match(/\n/g)||[]).length+1;n.textContent=Array.from({length:c},(_,i)=>i+1).join("\n")}
function drawRecs(){let b=$("#scriptRecs");if(!b)return;let r=scriptRecs();b.innerHTML=r.length?r.map(p=>`<button type="button" data-script-open="${esc(p)}">${esc(p)}</button>`).join(""):"<span class='muted'>No recent scripts</span>"}
async function saveScript(to){let p=to||pth(),t=$("#scriptText")?.value||"",body=new URLSearchParams();body.set("key",key);body.set("path",p);body.set("content",t);let r=await fetch("/save",{method:"POST",credentials:"same-origin",headers:{"Content-Type":"application/x-www-form-urlencoded"},body});let ok=r.ok&&(await r.text()).includes("Saved");if(ok){$("#scriptPath").value=p;saveRec(p)}setScriptOut(ok?"Saved "+p:"save failed: HTTP "+r.status)}
async function openScript(p){if(p)$("#scriptPath").value=p;p=pth();let out=await apiCmd("cat "+p);if(out.startsWith("cat:"))setScriptOut(out);else{$("#scriptText").value=out.replace(/\n$/,"");lineNums();saveRec(p);setScriptOut("Opened "+p)}}
function addScripts(){
$("#tabs")?.insertAdjacentHTML("beforeend",`<button class="tab" data-tab="scripts">Scripts</button>`);
$("#panels")?.insertAdjacentHTML("beforeend",`<section class="panel" id="scripts"><section class="card"><h2>Script Editor</h2><div class="scriptBar"><input id="scriptPath" value="/home/test.sh"><button id="scriptOpen" type="button">Open</button><button id="scriptSave" type="button">Save</button><button id="scriptValidate" type="button">Validate</button><button id="scriptRun" type="button">Run</button></div><div class="scriptBar"><input id="scriptSaveAs" placeholder="/home/copy.sh"><button id="scriptSaveAsBtn" type="button">Save as</button><button id="scriptNew" type="button" class="secondary">Clear/New</button></div><div class="scriptEdit"><pre id="scriptLines">1</pre><textarea id="scriptText" spellcheck="false" placeholder="# one KernelESP command per line"></textarea></div><h2>Recent Scripts</h2><div id="scriptRecs" class="quick"></div><pre id="scriptOut"></pre></section></section>`);
drawRecs();lineNums();if(window.kespEnhancePre)window.kespEnhancePre();
}
let oldStatus=refreshStatus;refreshStatus=async()=>{await oldStatus();paintBadges()};
let oldLive=refreshLive;refreshLive=async()=>{await oldLive();paintSensor()};
let oldRelays=relays;relays=rs=>{oldRelays(rs);setTimeout(paintRelays,0)};
function badge(t,k){return`<span class="kbadge ${k}">${esc(t)}</span>`}
function paintBadges(){$$(".stat").forEach(a=>{let l=$("span",a)?.textContent,v=$("strong",a);if(!l||!v||v.dataset.badge)return;let t=v.textContent,k="note";if(l=="Wi-Fi")k=t=="connected"?"ok":"bad";else if(l=="Armed")k=t=="on"?"ok":"warn";else if(l=="Time")k=t=="synced"?"ok":"warn";else if(l=="Heap"||l=="LittleFS"||l=="Max block")k="ok";v.dataset.badge=1;v.innerHTML=badge(t,k)})}
function paintRelays(){$$("#relays .relay").forEach(r=>$$(".pill",r).forEach(p=>{let t=p.textContent;p.classList.toggle("on",t=="ON");p.classList.toggle("off",t=="OFF")}))}
function paintSensor(){let s=$("#sensor"),h=s?.closest(".card")?.querySelector("h2");if(!s||!h)return;let old=$("#sensorState");if(old)old.remove();let t=s.textContent,k=t.includes("not found")||t.includes("error")?"bad":"ok";h.insertAdjacentHTML("beforeend",` <span id="sensorState" class="kbadge ${k}">${k=="ok"?"detected":"not found"}</span>`)}
document.addEventListener("click",async e=>{let o=e.target.closest("[data-script-open]");if(o){await openScript(o.dataset.scriptOpen);return}if(e.target.id=="scriptOpen")await openScript();if(e.target.id=="scriptSave")await saveScript();if(e.target.id=="scriptSaveAsBtn")await saveScript($("#scriptSaveAs").value.trim());if(e.target.id=="scriptValidate")setScriptOut("$ sh -n "+pth()+"\n"+await apiCmd("sh -n "+pth()));if(e.target.id=="scriptRun")setScriptOut("$ sh "+pth()+"\n"+await apiCmd("sh "+pth()));if(e.target.id=="scriptNew"){$("#scriptText").value="";lineNums();setScriptOut("")}},true);
document.addEventListener("input",e=>{if(e.target.id=="scriptText")lineNums()});
document.addEventListener("scroll",e=>{if(e.target.id=="scriptText")$("#scriptLines").scrollTop=e.target.scrollTop},true);
addScripts();
setTimeout(()=>{paintBadges();paintSensor();paintRelays()},1200);
})();
