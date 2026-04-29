(() => {
  const q = new URLSearchParams(location.search);
  const key = q.get("key") || "";
  const topics = [
    "index",
    "quickstart",
    "hardware",
    "relay",
    "sensor",
    "climate",
    "cron",
    "email",
    "mail",
    "inputs",
    "scripts",
    "web",
    "wifi",
    "files",
    "backup",
    "memory",
    "commands",
    "professional",
    "board",
    "release",
    "security",
    "safety",
    "troubleshooting"
  ];
  const names = {
    index: "Overview",
    quickstart: "Quick start",
    hardware: "Hardware",
    relay: "Relays",
    sensor: "Sensors",
    climate: "Climate",
    cron: "Cron",
    email: "Email",
    mail: "Mail command",
    inputs: "Inputs",
    scripts: "Scripts",
    web: "Web",
    wifi: "Wi-Fi",
    files: "Files",
    backup: "Backup",
    memory: "Memory",
    commands: "Commands",
    professional: "Professional",
    board: "Board",
    release: "Release",
    security: "Security",
    safety: "Safety",
    troubleshooting: "Troubleshooting"
  };

  function topic(anchor) {
    return anchor.dataset.topic || new URL(anchor.href, location.href).searchParams.get("topic") || "index";
  }

  function href(topicName) {
    return "/help?" + (key ? "key=" + encodeURIComponent(key) + "&" : "") + "topic=" + encodeURIComponent(topicName);
  }

  function pins() {
    const current = q.get("topic") || "index";
    if (current != "board" && current != "hardware") return;
    if (document.getElementById("pinouts")) return;
    const pre = document.querySelector("section.card>pre");
    if (!pre) return;
    pre.insertAdjacentHTML(
      "beforebegin",
      `<div id="pinouts" class="pinouts"><h3>Pinout</h3><a href="/pinout-esp12f.svg"><img src="/pinout-esp12f.svg" alt="ESP-12F pinout"></a><a href="/pinout-esp8266-boards.svg"><img src="/pinout-esp8266-boards.svg" alt="NodeMCU and Wemos D1 mini pinout"></a><a href="/pinout-esp01.svg"><img src="/pinout-esp01.svg" alt="ESP-01 pinout"></a></div>`
    );
    const style = document.createElement("style");
    style.textContent = ".pinouts{display:grid;gap:14px;margin:14px 0}.pinouts img{width:100%;max-width:760px;border:1px solid #cbd5e1;border-radius:8px;background:#f8fafc}.pinouts h3{margin:0}";
    document.head.append(style);
  }

  function apply() {
    const list = document.querySelector("section.card>p");
    if (!list) return;
    if (!list.dataset.fullHelp) {
      list.dataset.fullHelp = "1";
      topics.forEach(topicName => {
        if ([...list.querySelectorAll("a")].some(anchor => topic(anchor) == topicName)) return;
        const anchor = document.createElement("a");
        anchor.className = "btn secondary";
        anchor.dataset.topic = topicName;
        list.appendChild(anchor);
      });
    }
    topics.forEach(topicName => {
      const anchor = [...list.querySelectorAll("a")].find(candidate => topic(candidate) == topicName);
      if (anchor) list.appendChild(anchor);
    });
    [...list.querySelectorAll("a")].forEach(anchor => {
      const topicName = topic(anchor);
      anchor.dataset.topic = topicName;
      anchor.href = href(topicName);
      anchor.textContent = names[topicName] || topicName;
      anchor.classList.toggle("active", topicName == (q.get("topic") || "index"));
    });
    pins();
  }

  window.kespHelpI18nApply = apply;
  apply();
})();
