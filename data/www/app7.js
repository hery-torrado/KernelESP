(function(){
const cmds=["help","health","free","df","date","ntp status","ntp kick","wifi status","wifi net","ifconfig","ip route","relay status","rule list","crontab -l","timer list","jobs","dmesg","sensor read","i2c scan"];
function addOps(){
$("#tabs")?.insertAdjacentHTML("beforeend",`<button class="tab" data-tab="ops">Quick Cmds</button>`);
$("#panels")?.insertAdjacentHTML("beforeend",`<section class="panel" id="ops"><section class="card"><h2>Command Shortcuts</h2><div class="miniPanel"><button data-cmd="relay status">Relays</button><button data-cmd="rule list">Rules</button><button data-cmd="crontab -l">Cron</button><button data-cmd="timer list">Timers</button><button data-cmd="input list">Inputs</button><button data-cmd="health">Health</button></div><pre class="formOut"></pre></section><section class="card"><h2>Templates</h2><div class="miniPanel"><button data-fill="relay add relay1 D1 active_low">relay add</button><button data-fill="schedule relay1 08:00 08:10">daily schedule</button><button data-fill="climate temp fan 38 40">temp rule</button><button data-fill="climate hum extractor 60 70">humidity rule</button><button data-fill="input add button D2 pullup">input add</button><button data-fill="timer every 60000 health">timer health</button></div></section></section>`);
if(window.kespEnhancePre)window.kespEnhancePre();
}
document.addEventListener("click",e=>{let f=e.target.closest("[data-fill]");if(f){let i=$("#cmdInput");if(i){i.value=f.dataset.fill;i.focus()}}},true);
document.addEventListener("keydown",e=>{let i=e.target;if(i.id!="cmdInput")return;let h=typeof hist=="function"?hist():[],k=e.key;if(k=="ArrowUp"&&h.length){e.preventDefault();i.dataset.hi=String(Math.min(Number(i.dataset.hi||-1)+1,h.length-1));i.value=h[Number(i.dataset.hi)]}else if(k=="ArrowDown"&&h.length){e.preventDefault();let n=Math.max(Number(i.dataset.hi||0)-1,-1);i.dataset.hi=String(n);i.value=n<0?"":h[n]}else if(k=="Tab"){let v=i.value.trim(),m=cmds.find(c=>c.startsWith(v));if(m){e.preventDefault();i.value=m}}});
addOps();
})();
