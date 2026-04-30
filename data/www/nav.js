(()=>{
function canon(u){let p=u.pathname;if(p=="/index.html")return"/";if(p=="/cmd")return"/";if(p=="/edit"||p=="/run"||p=="/save"||p=="/delete"||p=="/wizard"||p=="/relays"||p=="/relay")return"/automations";return p}
function logout(nav){if(nav.querySelector('a[href="/logout"]'))return;let a=document.createElement("a");a.href="/logout";a.textContent="Logout";a.dataset.navLogout="1";nav.appendChild(a)}
function mark(){let nav=document.querySelector("header nav");if(!nav)return;logout(nav);let h=canon(new URL(location.href));nav.querySelectorAll("a").forEach(a=>{let p=canon(new URL(a.href,location.href)),on=p==h&&p!="/logout";a.classList.toggle("active",on);on?a.setAttribute("aria-current","page"):a.removeAttribute("aria-current")})}
if(!document.getElementById("navStyle"))document.head.insertAdjacentHTML("beforeend",`<style id="navStyle">nav a.active,nav a[aria-current=page]{background:var(--brand);border-color:var(--brand);color:#fff;font-weight:800}nav a[data-nav-logout]{margin-left:auto}</style>`);
mark();addEventListener("hashchange",mark)
})();
