(function(){
function addMailUi(){
  $("#tabs")?.insertAdjacentHTML("beforeend",`<button class="tab" data-tab="mail">Mail</button>`);
  $("#panels")?.insertAdjacentHTML("beforeend",`<section class="panel" id="mail">
<div class="grid">
<section class="card"><h2>Mail Alerts</h2><pre id="mailStatusRaw" data-live="mail status"></pre><form id="mailConfigForm"><input id="mailHost" name="host" placeholder="SMTP host"><input id="mailPort" name="port" inputmode="numeric" value="25" placeholder="port"><input id="mailFrom" name="from" placeholder="from address"><input id="mailTo" name="to" placeholder="default recipient"><input id="mailHelo" name="helo" placeholder="HELO name"><button>Save</button></form><div class="quick"><button type="button" id="mailReload">Reload</button></div><pre class="formOut"></pre></section>
<section class="card"><h2>Send Mail</h2><form id="mailTestForm"><input name="message" value="Manual test from KernelESP"><button>Test Email</button></form><form id="mailHealthForm"><input name="subject" value="KernelESP daily health"><button>Send Health Report</button></form><pre class="formOut"></pre></section>
</div>
<div class="grid">
<section class="card"><h2>Daily Health Email</h2><form id="mailDailyForm"><input name="time" type="text" inputmode="numeric" pattern="^([01][0-9]|2[0-3]):[0-5][0-9]$" placeholder="HH:MM" value="08:00" data-clock><input name="subject" value="KernelESP daily health"><button>Add Daily Job</button></form><div class="quick"><button data-cmd="crontab -l">Cron Entries</button></div><pre class="formOut"></pre></section>
<section class="card"><h2>Sensor Email Rule</h2><form id="mailRuleForm"><select name="metric"><option value="temp">temperature</option><option value="hum">humidity</option><option value="press">pressure</option></select><select name="op"><option value="gt">above</option><option value="lt">below</option></select><input name="threshold" value="40"><input name="cooldown" value="300000"><input name="subject" value="KernelESP sensor alert"><input name="message" value="Sensor threshold reached."><button>Add Rule</button></form><pre class="formOut"></pre></section>
</div>
<div class="grid">
<section class="card"><h2>Fan Heat Workflow</h2><form id="mailFanForm"><input name="relay" value="fan"><span class="pinPick"><select data-pin-select><option>D5</option><option>D1</option><option>D2</option><option>D6</option><option>D7</option><option>D0</option><option>D3</option><option>D4</option><option>D8</option><option>GPIO4</option><option>GPIO5</option><option>custom</option></select><input name="pin" value="D5" aria-label="pin"></span><select name="mode"><option value="active_low">active_low</option><option value="active_high">active_high</option></select><input name="low" value="38"><input name="high" value="40"><input name="cooldown" value="300000"><input name="onSubject" value="KernelESP heat alert"><input name="onMessage" value="Temperature is above 40 C. Fan is on."><input name="offSubject" value="KernelESP temperature normal"><input name="offMessage" value="Temperature is below 38 C. Fan is off."><button>Create Workflow</button></form><pre class="formOut"></pre></section>
<section class="card"><h2>Input Email Alert</h2><form id="mailInputForm"><input name="name" value="power"><span class="pinPick"><select data-pin-select><option>D2</option><option>D1</option><option>D5</option><option>D6</option><option>D7</option><option>D0</option><option>D3</option><option>D4</option><option>D8</option><option>GPIO4</option><option>GPIO5</option><option>custom</option></select><input name="pin" value="D2" aria-label="pin"></span><select name="mode"><option value="pullup">pullup</option><option value="float">float</option></select><select name="state"><option value="low">low</option><option value="high">high</option><option value="change">change</option></select><input name="debounce" value="50"><input name="subject" value="KernelESP power alert"><input name="message" value="Power flow has stopped."><button>Create Input Alert</button></form><pre class="formOut"></pre></section>
</div>
</section>`);
}
addMailUi();
import("/app13.js?v=3");
})();
