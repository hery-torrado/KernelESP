function addNetConfig(){
let n=$("#net");if(!n)return;
n.insertAdjacentHTML("beforeend",`<section class="card"><h2>Static IP</h2><form data-run="wifi static {ip} {gw} {mask} {dns1} {dns2}"><input name="ip" placeholder="192.168.0.107"><input name="gw" placeholder="192.168.0.1"><input name="mask" placeholder="255.255.255.0"><input name="dns1" placeholder="1.1.1.1"><input name="dns2" placeholder="8.8.8.8"><button>Save static</button></form><div class="quick"><button data-cmd="wifi net">Show config</button><button data-cmd="wifi dhcp on">Use DHCP</button><button data-cmd="wifi reconnect">Reconnect</button></div><p class="muted">Save first, then reconnect when ready.</p><pre class="formOut"></pre></section>`);
}
function keepKey(){if(!key)return;$$("nav a").forEach(a=>{let u=new URL(a.href,location.href);u.searchParams.set("key",key);a.href=u.pathname+u.search})}
keepKey();
addNetConfig();
