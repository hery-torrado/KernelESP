(function(){
function clean(v){return String(v||"").replace(/[\r\n]+/g," ").replace(/["']/g,"").trim()}
function q(v){return `"${clean(v)}"`}
function safe(v,d){v=clean(v).toLowerCase().replace(/[^a-z0-9_.-]+/g,"-").replace(/^-+|-+$/g,"");return v||d}
function field(f,n,d=""){let e=f.elements[n];return clean(e?e.value:d)}
function scriptPath(prefix,name){return `/home/${prefix}-${safe(name,"alert")}.sh`}
async function runSeq(cmds,out){
  out.textContent="";
  for(let c of cmds){if(!c)continue;if(window.saveHist)saveHist(c);out.textContent+="$ "+c+"\n";out.textContent+=(await apiCmd(c)).replace(/\s+$/,"")+"\n"}
  await refreshAll(false);
}
function parseStatus(t){let m={};String(t||"").split(/\r?\n/).forEach(l=>{let i=l.indexOf("=");if(i>0)m[l.slice(0,i)]=l.slice(i+1)});return m}
async function loadMailStatus(){
  let p=$("#mailStatusRaw");if(!p)return;
  let t=await apiCmd("mail status");p.textContent=t;
  let s=parseStatus(t),map={mailHost:"mail.smtp.host",mailPort:"mail.smtp.port",mailFrom:"mail.from",mailTo:"mail.to",mailHelo:"mail.helo"};
  Object.keys(map).forEach(id=>{let e=$("#"+id);if(e&&s[map[id]]!==undefined)e.value=s[map[id]]});
}
async function fanWorkflow(f,o){
  let r=safe(field(f,"relay","fan"),"fan"),on=scriptPath("heat-on",r),off=scriptPath("heat-off",r);
  await runSeq([`relay add ${r} ${field(f,"pin","D5")} ${field(f,"mode","active_low")}`,`relay boot ${r} off`,"sensor begin",`rule cooldown ${field(f,"cooldown","300000")}`,`write ${on} relay on ${r}`,`append ${on} mail send default ${q(field(f,"onSubject","KernelESP heat alert"))} ${q(field(f,"onMessage","Temperature is above 40 C. Fan is on."))}`,`write ${off} relay off ${r}`,`append ${off} mail send default ${q(field(f,"offSubject","KernelESP temperature normal"))} ${q(field(f,"offMessage","Temperature is below 38 C. Fan is off."))}`],o);
  let add=`rule add temp range ${field(f,"low","38")} ${field(f,"high","40")} sh ${on}`;
  o.textContent+="$ "+add+"\n";let res=await apiCmd(add);o.textContent+=res.replace(/\s+$/,"")+"\n";
  let m=res.match(/rule id\s+(\d+)/i);
  if(m)await runSeq([`rule off ${m[1]} sh ${off}`,"rule list"],o);
  else o.textContent+="rule off skipped: rule id was not returned\n";
}
document.addEventListener("click",async e=>{
  if(e.target.id=="mailReload")await loadMailStatus();
  let t=e.target.closest(".tab");if(t?.dataset.tab=="mail")setTimeout(loadMailStatus,80);
},true);
document.addEventListener("submit",async e=>{
  let f=e.target;
  if(!/^mail/.test(f.id))return;
  let o=outFor(f);
  if(f.id=="mailConfigForm"){e.preventDefault();await runSeq([`mail config ${field(f,"host")} ${field(f,"port","25")} ${field(f,"from")} ${field(f,"to")}`,`mail helo ${field(f,"helo","kernelesp")}`],o);await loadMailStatus();return}
  if(f.id=="mailTestForm"){e.preventDefault();await runSeq([`mail test ${q(field(f,"message","Manual test from KernelESP"))}`],o);return}
  if(f.id=="mailHealthForm"){e.preventDefault();await runSeq([`mail health ${q(field(f,"subject","KernelESP daily health"))}`],o);return}
  if(f.id=="mailDailyForm"){e.preventDefault();await runSeq(["ntp sync",`cron add daily ${field(f,"time","08:00")} mail health ${q(field(f,"subject","KernelESP daily health"))}`,"crontab -l"],o);return}
  if(f.id=="mailRuleForm"){e.preventDefault();await runSeq([`rule cooldown ${field(f,"cooldown","300000")}`,`rule add ${field(f,"metric","temp")} ${field(f,"op","gt")} ${field(f,"threshold","40")} mail send default ${q(field(f,"subject","KernelESP sensor alert"))} ${q(field(f,"message","Sensor threshold reached."))}`,"rule list"],o);return}
  if(f.id=="mailFanForm"){e.preventDefault();await fanWorkflow(f,o);return}
  if(f.id=="mailInputForm"){e.preventDefault();let n=safe(field(f,"name","power"),"power"),p=scriptPath("input-alert",n);await runSeq([`input add ${n} ${field(f,"pin","D2")} ${field(f,"mode","pullup")} ${field(f,"debounce","50")}`,`write ${p} mail send default ${q(field(f,"subject","KernelESP input alert"))} ${q(field(f,"message","Input alert triggered."))}`,`input on ${n} ${field(f,"state","low")} sh ${p}`,"input list"],o);return}
},true);
})();
