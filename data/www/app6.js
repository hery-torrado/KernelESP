(function(){
const PKEY="kesp.retro";
document.head.insertAdjacentHTML("beforeend",`<style>
.netdot{display:inline-flex;align-items:center;gap:6px;border:1px solid var(--line);border-radius:999px;padding:6px 9px;background:#f6f8fb;color:#344054;font-size:12px;font-weight:750}.netdot:before{content:"";width:9px;height:9px;border-radius:50%;background:#b8c0cc}.netdot.ok:before{background:#23c66b;box-shadow:0 0 8px #23c66b}.netdot.bad:before{background:#d14343;box-shadow:0 0 8px #d14343}.netdot.wait:before{background:#d9a514}
.copyBtn{float:right;margin-top:-4px;background:#2e3b47;font-size:12px;min-height:28px;padding:5px 8px}.preTools{display:flex;justify-content:flex-end;margin:-2px 0 6px}
.topOutCard.hide{display:none}
body.retro{--bg:#07120b;--panel:#0b1710;--ink:#d9ffe3;--muted:#90caa1;--line:#1d4e2d;--brand:#39ff66;--brand2:#126c32;--code:#020703;--codeText:#39ff66}
body.retro header,body.retro nav a{background:#030803;border-color:#1d7c39;color:#baffc8}body.retro .card{box-shadow:0 0 16px rgba(57,255,102,.08)}body.retro pre{text-shadow:0 0 6px rgba(57,255,102,.55)}
.miniPanel{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:10px}.miniPanel button{justify-content:flex-start}
</style>`);
function conn(s){let d=$("#netDot");if(d){d.className="netdot "+s;d.textContent=s=="ok"?"online":s=="bad"?"offline":"checking"}}
let oldApiJson=apiJson;apiJson=async p=>{conn("wait");try{let j=await oldApiJson(p);conn("ok");return j}catch(e){conn("bad");throw e}};
function theme(on){document.body.classList.toggle("retro",on);localStorage.setItem(PKEY,on?"1":"0")}
function copyText(t){if(navigator.clipboard)navigator.clipboard.writeText(t);else{let a=document.createElement("textarea");a.value=t;document.body.append(a);a.select();document.execCommand("copy");a.remove()}}
function enhancePre(){document.querySelectorAll(".card").forEach(c=>{if(c.dataset.tools||!c.querySelector("pre"))return;c.dataset.tools=1;c.insertAdjacentHTML("afterbegin",`<div class="preTools"><button type="button" class="copyBtn">copy</button></div>`)})}
window.kespEnhancePre=enhancePre;
$(".actions")?.insertAdjacentHTML("afterbegin",`<span id="netDot" class="netdot wait">checking</span><button id="retroBtn" type="button">Retro</button>`);
$(".toolbar")?.insertAdjacentHTML("afterend",`<section class="card topOutCard hide"><h2>Output</h2><pre id="topOut"></pre></section>`);
theme(localStorage.getItem(PKEY)=="1");conn("wait");enhancePre();setInterval(enhancePre,15000);
document.addEventListener("click",async e=>{let top=e.target.closest(".toolbar [data-cmd]");if(top){e.preventDefault();e.stopImmediatePropagation();let c=top.dataset.cmd,o=$("#topOut"),box=$(".topOutCard");if(o&&box){box.classList.remove("hide");o.textContent="$ "+c+"\\n"+await apiCmd(c);await refreshAll(false)}return}if(e.target.id=="retroBtn")theme(!document.body.classList.contains("retro"));let b=e.target.closest(".copyBtn");if(b){let ps=$$("pre",b.closest(".card")).map(p=>p.textContent).join("\\n\\n");copyText(ps);b.textContent="copied";setTimeout(()=>b.textContent="copy",900)}},true);
})();
