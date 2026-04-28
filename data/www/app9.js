(function(){
const A=["rule list","crontab -l","timer list","input list"];
function b(t,k){return`<span class="kbadge ${k}">${esc(t)}</span>`}
function rows(txt){let a=String(txt||"").split(/\r?\n/).map(x=>x.trim()).filter(Boolean).filter(x=>!x.startsWith("every:")&&!x.startsWith("epoch:")&&!x.startsWith("(no ")&&!/^\d{4}-/.test(x));return a.length?a.map(x=>`<div class="autoRow"><code>${esc(x)}</code></div>`).join(""):"<p class='muted'>(none)</p>"}
function affected(all){let m=[...String(all).matchAll(/relay\s+(?:on|off|pulse|toggle)\s+([A-Za-z0-9_.-]+)/g)].map(x=>x[1]);m=[...new Set(m)];return m.length?m.map(x=>b(x,"note")).join(" "):"<span class='muted'>(none)</span>"}
async function autoView(){
let s=await apiJson("/api/status"),rel=s.relays||[];
$("#autoRelays").innerHTML=rel.length?rel.map(r=>`<div class="autoRow"><strong>${esc(r.name)}</strong>${b(r.state?"ON":"OFF",r.state?"ok":"bad")}<span class="muted">GPIO${esc(r.pin)}</span></div>`).join(""):"<p class='muted'>(no relays)</p>";
let all="";
for(let c of A){let t=await apiCmd(c);all+=t+"\n";let id=c.startsWith("rule")?"autoRules":c.startsWith("crontab")?"autoCron":c.startsWith("timer")?"autoTimers":"autoInputs";$("#"+id).innerHTML=rows(t)}
$("#autoAffected").innerHTML=affected(all);
}
async function runDiag(){
let cmds=["health","free","df","wifi status","date","sensor read","relay status","rule list","crontab -l","timer list","input list","dmesg"];
$("#diagOut").innerHTML="<p class='muted'>Running...</p>";
let html="";
for(let c of cmds){let o=await apiCmd(c);html+=`<section class="diagItem"><h2>${esc(c)}</h2><pre>${esc(o)}</pre></section>`}
$("#diagOut").innerHTML=html;if(window.kespEnhancePre)window.kespEnhancePre();
}
function addAutoDiag(){
$("#tabs")?.insertAdjacentHTML("beforeend",`<button class="tab" data-tab="autov">Automations</button><button class="tab" data-tab="diagv">Diagnostics</button>`);
$("#panels")?.insertAdjacentHTML("beforeend",`<section class="panel" id="autov"><section class="card"><h2>Automation View</h2><p><button id="autoRefresh" type="button">Refresh</button></p><div class="autoGrid"><article><h2>Relays</h2><div id="autoRelays"></div></article><article><h2>Rules</h2><div id="autoRules"></div></article><article><h2>Cron</h2><div id="autoCron"></div></article><article><h2>Timers</h2><div id="autoTimers"></div></article><article><h2>Inputs</h2><div id="autoInputs"></div></article><article><h2>Affected relays</h2><div id="autoAffected"></div></article></div></section></section><section class="panel" id="diagv"><section class="card"><h2>Diagnostics</h2><p><button id="runDiag" type="button">Run diagnostics</button></p><div id="diagOut" class="diagGrid"></div></section></section>`);
}
document.addEventListener("click",async e=>{if(e.target.id=="autoRefresh")await autoView();if(e.target.id=="runDiag")await runDiag();let t=e.target.closest(".tab");if(t?.dataset.tab=="autov")setTimeout(autoView,80)},true);
setInterval(()=>{if($("#autov")?.classList.contains("active"))autoView()},12000);
addAutoDiag();
})();
