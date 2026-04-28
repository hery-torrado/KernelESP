(function(){
const profs={
generic:{label:"Generic ESP8266",safe:"D1 D2 D5 D6 D7",risky:"D0 D3 D4 D8 RX TX GPIO6-GPIO11",i2c:"D2=SDA, D1=SCL",relay:"active_low is common, verify with pulse"},
nodemcu:{label:"NodeMCU / ESP-12E",safe:"D1 D2 D5 D6 D7",risky:"D0 D3 D4 D8 RX TX",i2c:"D2=SDA, D1=SCL",relay:"D1 first relay, D5-D7 for extra relays"},
d1mini:{label:"Wemos D1 mini",safe:"D1 D2 D5 D6 D7",risky:"D0 D3 D4 D8 RX TX",i2c:"D2=SDA, D1=SCL",relay:"D1/D5 usually easiest for relays"},
esp12f:{label:"ESP-12F module",safe:"GPIO4 GPIO5 GPIO12 GPIO13 GPIO14",risky:"GPIO0 GPIO2 GPIO15 GPIO16 RX TX GPIO6-GPIO11",i2c:"GPIO4=SDA, GPIO5=SCL",relay:"prefer GPIO5/GPIO12/GPIO13/GPIO14; avoid boot pins"},
esp01:{label:"ESP-01",safe:"GPIO2 after boot",risky:"GPIO0 GPIO1/TX GPIO3/RX",i2c:"external wiring required",relay:"use a tested ESP-01 relay adapter"}};
const diagCmds=["version","uname","health","free","df","flash","wifi status","wifi net","date","ntp status","sensor read","relay status","rule list","crontab -l","timer list","input list","dmesg"];
function line(k,v){return `<div class="kv"><span>${esc(k)}</span><strong>${esc(v)}</strong></div>`}
function dl(name,text){let a=document.createElement("a"),u=URL.createObjectURL(new Blob([text],{type:"text/plain;charset=utf-8"}));a.href=u;a.download=name;document.body.append(a);a.click();setTimeout(()=>{URL.revokeObjectURL(u);a.remove()},1000)}
async function diagText(){let s=await apiJson("/api/status"),out=`KernelESP diagnostic bundle\ncreated=${new Date().toISOString()}\nbase=${location.origin}\n\n== api/status ==\n${JSON.stringify(s,null,2)}\n\n`;for(let c of diagCmds)out+=`== ${c} ==\n${await apiCmd(c)}\n`;return out}
async function runPreflight(){let o=$("#proOut");o.textContent="Running...";let s=await apiJson("/api/status"),rows=[["Version",s.version],["Wi-Fi",s.wifi],["IP",s.ip],["Heap",fmtBytes(s.heap)],["Max block",fmtBytes(s.max_block)],["LittleFS free",fmtBytes(s.fs_free)],["Time",s.epoch>1600000000?"synced":"not synced"],["Armed",s.armed]];o.innerHTML=rows.map(x=>line(x[0],x[1])).join("")+"<pre>"+esc(await apiCmd("health"))+"</pre>"}
function profileName(){return $("#boardProfile")?.value||"generic"}
function paintProfile(name=profileName()){let p=profs[name]||profs.generic,b=$("#pinGuide");if(!b)return;b.innerHTML=[["Profile",p.label],["Safe pins",p.safe],["Risky pins",p.risky],["Recommended I2C",p.i2c],["Relay default",p.relay]].map(x=>line(x[0],x[1])).join("")}
async function loadProfile(){let v=(await apiCmd("config get board.profile")).trim();if(!profs[v])v="generic";let s=$("#boardProfile");if(s)s.value=v;paintProfile(v)}
async function saveProfile(){let n=profileName(),o=$("#profileOut");o.textContent=await apiCmd("config set board.profile "+n);paintProfile(n)}
window.kespPro={profs,line,dl,diagText,runPreflight,paintProfile,loadProfile,saveProfile};
})();
