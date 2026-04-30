(()=>{if(window.kespApp8bLoaded)return;window.kespApp8bLoaded=1;
let oldStatus=refreshStatus;refreshStatus=async()=>{await oldStatus();paintBadges()};
let oldLive=refreshLive;refreshLive=async()=>{await oldLive();paintSensor()};
let oldRelays8b=relays;relays=rs=>{oldRelays8b(rs);setTimeout(paintRelays,0)};
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
