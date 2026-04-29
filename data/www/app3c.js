window.kespStart=function(){
document.addEventListener("click",async e=>{let t=e.target.closest(".tab");if(t){$$(".tab").forEach(x=>x.classList.toggle("active",x===t));$$(".panel").forEach(p=>p.classList.toggle("active",p.id===t.dataset.tab));refreshLive();return}let c=e.target.closest("[data-cmd]");if(c){let o=outFor(c);if(o)o.textContent="$ "+c.dataset.cmd+"\\n"+await apiCmd(c.dataset.cmd);await refreshAll(false);return}let r=e.target.closest("[data-relay]");if(r){try{await relayAction(r.dataset.relay,r.dataset.state)}catch(err){let o=outFor(r);if(o)o.textContent="error: "+err.message}return}if(e.target.id==="refresh")await refreshAll()});
document.addEventListener("change",e=>{let p=e.target.closest("[data-clock-pick]");if(p)syncClockPick(p)},true);
document.addEventListener("submit",async e=>{let f=e.target;if(f.id==="cmdForm"){e.preventDefault();$("#cmdOut").textContent=await apiCmd($("#cmdInput").value);await refreshAll(false);return}if(f.dataset.run){e.preventDefault();if(!validateClockFields(f))return;let c=fill(f.dataset.run,f),o=outFor(f);if(o)o.textContent="$ "+c+"\\n"+await apiCmd(c);await refreshAll(false)}});
if(window.kespReady)window.kespBoot();
};
window.kespStart();
