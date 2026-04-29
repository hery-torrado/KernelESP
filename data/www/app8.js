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
