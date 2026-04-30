(function(){
const KEY="kesp.lang",langs={en:"English",es:"Espa\u00f1ol",pt:"Portugu\u00eas"},dict=window.KESP_I18N||{};let cur="";
let reverse={};for(let l of Object.keys(dict))for(let k of Object.keys(dict[l]))reverse[dict[l][k]]=k;
function stored(){try{return localStorage.getItem(KEY)||""}catch(e){return""}}
function save(l){try{localStorage.setItem(KEY,l)}catch(e){}}
function lang(){if(cur)return cur;let q=new URLSearchParams(location.search).get("lang"),s=stored();cur=q&&langs[q]?q:(s&&langs[s]?s:"en");save(cur);return cur}
function source(s){return reverse[s]||s}
function patch(s,d){for(let k of ["not connected","max block","not synced","connected","connecting","synced","rules","crons","heap","frag","on","off"])if(d[k])s=s.replace(new RegExp("(^|[^A-Za-z0-9_])("+k+")(?=$|[^A-Za-z0-9_])","g"),(m,a)=>a+d[k]);return s}
function text(s,l){let src=source(s),d=dict[l]||{};if(l=="en")return src;if(d[src])return d[src];for(let p of ["Saved ","Opened ","Heap ","Run ","Delete ","save failed: ","offline or unauthorized: ","Low heap: ","Heap fragmentation: "]){if(src.startsWith(p)&&d[p])return d[p]+src.slice(p.length)}return patch(src,d)}
function skip(n){let e=n.nodeType==3?n.parentElement:n;if(!e)return true;return !!e.closest("script,style,pre,code,textarea")}
function translateNode(n,l){let m=String(n.nodeValue).match(/^(\s*)(.*?)(\s*)$/s);if(!m||!m[2])return;let v=text(m[2],l);if(v!==m[2])n.nodeValue=m[1]+v+m[3]}
function translateAttrs(root,l){let q=root.nodeType==1?root:document;let a=[];if(root.nodeType==1&&root.matches?.("[placeholder],[title],[aria-label]"))a.push(root);a.push(...q.querySelectorAll?.("[placeholder],[title],[aria-label]")||[]);a.forEach(e=>["placeholder","title","aria-label"].forEach(k=>{let v=e.getAttribute(k);if(!v)return;let t=text(v,l);if(t!==v)e.setAttribute(k,t)}))}
function walk(root=document.body){let l=lang(),w=document.createTreeWalker(root,NodeFilter.SHOW_TEXT,{acceptNode:n=>skip(n)?NodeFilter.FILTER_REJECT:NodeFilter.FILTER_ACCEPT});let nodes=[];while(w.nextNode())nodes.push(w.currentNode);nodes.forEach(n=>translateNode(n,l));translateAttrs(root,l);document.documentElement.lang=l;document.querySelectorAll("[data-lang]").forEach(b=>b.classList.toggle("active",b.dataset.lang==l))}
function addSwitch(){if(document.querySelector(".langSwitch"))return;let nav=document.querySelector("header nav")||document.querySelector("header");if(!nav)return;let box=document.createElement("span");box.className="langSwitch";box.innerHTML=Object.keys(langs).map(l=>`<button type="button" data-lang="${l}">${langs[l]}</button>`).join("");nav.appendChild(box)}
function style(){if(document.getElementById("i18nStyle"))return;document.head.insertAdjacentHTML("beforeend",`<style id="i18nStyle">.langSwitch{display:flex;gap:5px;flex-wrap:wrap;margin-left:auto}.langSwitch button{min-height:30px;padding:6px 8px;margin:0;border:1px solid #314252;border-radius:6px;background:#1b2631;color:#eaf7fb;font-size:12px}.langSwitch button.active{background:var(--brand);border-color:var(--brand);color:#fff}@media(max-width:640px){.langSwitch{margin-left:0;width:100%}}</style>`)}
function apply(){style();addSwitch();walk();import("/nav.js?v=1");if(location.pathname=="/help")import("/i18n-help.js?v=19")}
document.addEventListener("click",e=>{let b=e.target.closest("[data-lang]");if(!b)return;e.preventDefault();cur=b.dataset.lang;save(cur);walk()});
new MutationObserver(ms=>{let l=lang();for(let m of ms)for(let n of m.addedNodes){if(n.nodeType==3&&!skip(n))translateNode(n,l);else if(n.nodeType==1&&!skip(n))walk(n)}}).observe(document.documentElement,{childList:true,subtree:true});
if(document.readyState=="loading")document.addEventListener("DOMContentLoaded",apply);else apply();
})();
