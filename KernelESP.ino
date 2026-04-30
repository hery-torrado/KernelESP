/*
  KernelESP

  SPDX-License-Identifier: BSD-3-Clause

  BSD 3-Clause License.

  Copyright (c) 2026, KernelESP contributors.
  Portions copyright (c) 2026, Arc1011 (KernelUNO).

  KernelESP is inspired by and includes source code copied/adapted from
  KernelUNO by Arc1011: https://github.com/Arc1011/KernelUNO
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <time.h>
#include <sys/time.h>

class StringCapture : public Print {
 public:
  StringCapture(String& target, size_t limitBytes) : target(target), limitBytes(limitBytes), droppedBytes(0) {}

  size_t write(uint8_t b) override {
    if (target.length() < limitBytes) target += (char)b;
    else droppedBytes++;
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    for (size_t i = 0; i < size; i++) write(buffer[i]);
    return size;
  }

  bool truncated() const {
    return droppedBytes > 0;
  }

 private:
  String& target;
  size_t limitBytes;
  size_t droppedBytes;
};

class KernelConsole : public Print {
 public:
  explicit KernelConsole(HardwareSerial& serial) : serial(serial), captureSink(nullptr), captureMute(false) {}

  void begin(unsigned long baud) {
    serial.begin(baud);
  }

  void setTimeout(unsigned long timeoutMs) {
    serial.setTimeout(timeoutMs);
  }

  int available() {
    return serial.available();
  }

  int read() {
    return serial.read();
  }

  size_t write(uint8_t b) override {
    size_t written = captureMute ? 1 : serial.write(b);
    if (captureSink) captureSink->write(b);
    return written;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    size_t written = captureMute ? size : serial.write(buffer, size);
    if (captureSink) captureSink->write(buffer, size);
    return written;
  }

  void capture(Print* sink, bool mute = false) {
    captureSink = sink;
    captureMute = mute;
  }

  void releaseCapture() {
    captureSink = nullptr;
    captureMute = false;
  }

  using Print::write;

 private:
  HardwareSerial& serial;
  Print* captureSink;
  bool captureMute;
};

KernelConsole KernelSerial(Serial);
#define Serial KernelSerial

#define KERNEL_NAME "KernelESP"
#define KERNEL_VERSION "0.10.0"
#define CONFIG_SCHEMA_VERSION "2"
#define SERIAL_BAUD 115200
#define MAX_LINE 192
#define MAX_ARGS 12
#define MAX_PIPE_STAGES 4
#define PIPE_CAPTURE_BYTES 5000
#define MAX_HISTORY 10
#define DMESG_LINES 16
#define DMESG_LEN 72
#define MAX_RELAYS 8
#define MAX_TIMERS 8
#define MAX_RULES 8
#define MAX_CRONS 8
#define MAX_INPUTS 6
#define MAX_ALIASES 8
#define MAX_ENV_VARS 8
#define CONF_CONFIG "/etc/config.txt"
#define CONF_RELAYS "/etc/relays.txt"
#define CONF_TIMERS "/etc/timers.txt"
#define CONF_RULES "/etc/rules.txt"
#define CONF_CRONS "/etc/crons.txt"
#define CONF_INPUTS "/etc/inputs.txt"
#define CONF_SCENES "/etc/scenes.txt"
#define CONF_STATE "/etc/state.txt"
#define CONF_DEFINES "/etc/defines.txt"
#define CONF_ALIASES "/etc/aliases"
#define HISTORY_FILE "/home/.history"
#define PKG_DIR "/pkg"
#define FUNC_DIR "/func"
#define PROFILE_DIR "/profiles"
#define LOG_FILE "/var/log/kernel.log"
#define SAFE_BOOT_FILE "/safe"
#define DEFAULT_BOOT_SCRIPT "/etc/boot.sh"
#define MOTD_FILE "/etc/motd"
#define LOG_MAX_BYTES 16384
#define LOG_KEEP_BYTES 8192
#define DEFAULT_WIFI_CONNECT_TIMEOUT_MS 45000UL
#define WIFI_WATCHDOG_INTERVAL_MS 60000UL
#define WIFI_AP_AFTER_FAILS 2
#define WIFI_ATTEMPT_EVENT_FAILS 12
#define WIFI_RETRY_MIN_MS 5000UL
#define WIFI_RETRY_MAX_MS 60000UL
#define NTP_RETRY_MIN_MS 10000UL
#define NTP_RETRY_MAX_MS 120000UL
#define NTP_HTTP_FALLBACK_RETRIES 2
#define WIFI_PROFILE_DIR "/profiles/wifi"

struct LogLine {
  unsigned long ms;
  char text[DMESG_LEN];
};

struct PinAlias {
  const char* name;
  int gpio;
  bool risky;
};

struct Relay {
  String name;
  int pin;
  bool activeLow;
  String bootState;
  bool configured;
  bool state;
};

struct TimerJob {
  uint8_t id;
  unsigned long intervalMs;
  unsigned long nextRun;
  String command;
  bool active;
  bool repeat;
};

struct Rule {
  uint8_t id;
  uint8_t metric;
  uint8_t op;
  float threshold;
  float threshold2;
  String command;
  String offCommand;
  bool active;
  bool latched;
  unsigned long cooldownMs;
  unsigned long lastRunMs;
};

struct CronJob {
  uint8_t id;
  uint8_t mode;
  uint8_t hour;
  uint8_t minute;
  uint8_t dowMask;
  uint8_t month;
  uint8_t day;
  int lastRunDay;
  String command;
  bool active;
};

struct AliasEntry {
  String name;
  String command;
  bool active;
};

struct EnvEntry {
  String name;
  String value;
  bool active;
};

struct InputWatcher {
  String name;
  int pin;
  bool pullup;
  bool active;
  bool stableState;
  bool lastRead;
  unsigned long lastChangeMs;
  unsigned long debounceMs;
  String highCommand;
  String lowCommand;
  String changeCommand;
};

struct BmeCalibration {
  uint16_t t1;
  int16_t t2, t3;
  uint16_t p1;
  int16_t p2, p3, p4, p5, p6, p7, p8, p9;
  uint8_t h1, h3;
  int16_t h2, h4, h5;
  int8_t h6;
};

struct SensorReading {
  bool ok;
  bool hasHumidity;
  float temperatureC;
  float pressureHpa;
  float humidityPct;
};

const PinAlias PIN_ALIASES[] = {
  {"D0", 16, false}, {"D1", 5, false}, {"D2", 4, false},
  {"D3", 0, true},  {"D4", 2, true},  {"D5", 14, false},
  {"D6", 12, false}, {"D7", 13, false}, {"D8", 15, true},
  {"RX", 3, true}, {"TX", 1, true}, {"LED", LED_BUILTIN, true},
};

char inputLine[MAX_LINE];
uint8_t inputLen = 0;
String cwd = "/";
String history[MAX_HISTORY];
uint8_t historyCount = 0;
bool suppressHistory = false;
LogLine dmesgRing[DMESG_LINES];
uint8_t dmesgHead = 0;
bool fsReady = false;
Relay relays[MAX_RELAYS];
TimerJob timers[MAX_TIMERS];
Rule rules[MAX_RULES];
CronJob crons[MAX_CRONS];
InputWatcher inputs[MAX_INPUTS];
AliasEntry aliases[MAX_ALIASES];
EnvEntry envVars[MAX_ENV_VARS];
uint8_t nextTimerId = 1;
uint8_t nextRuleId = 1;
uint8_t nextCronId = 1;
unsigned long ruleIntervalMs = 5000;
unsigned long nextRuleEvalMs = 0;
unsigned long ruleCooldownMs = 0;
unsigned long cronIntervalMs = 30000;
unsigned long nextCronEvalMs = 0;
unsigned long nextHealthCheckMs = 0;
ESP8266WebServer webServer(80);
bool webRunning = false;
bool bootFinished = false;
unsigned long relayPulseUntil[MAX_RELAYS];
BmeCalibration bmeCal;
bool sensorReady = false;
bool sensorHasHumidity = false;
uint8_t sensorAddress = 0x76;
int32_t bmeTFine = 0;
unsigned long lastNtpSyncMs = 0;
bool ntpSyncedOnce = false;
bool ntpPendingSync = false;
uint8_t ntpRetryCount = 0;
bool wifiConnecting = false;
bool wifiWasConnected = false;
unsigned long wifiConnectStartedMs = 0;
unsigned long nextWifiWatchMs = 0;
uint8_t wifiFailCount = 0;
int lastWifiDisconnectReason = 0;
uint8_t wifiAttemptDisconnectEvents = 0;
unsigned long wifiIgnoreDisconnectEventsUntilMs = 0;
unsigned long wifiLastConnectedMs = 0;
WiFiEventHandler wifiDisconnectedHandler;
bool fallbackApRunning = false;
bool automationsArmed = true;
bool ifBranchRunning = false;
uint8_t webAuthFails = 0;
unsigned long webAuthLockedUntil = 0;

void printPrompt();
void executeLine(String line);
bool hasPipeOutsideQuotes(const String& line);
void executePipeline(String line);
void runScriptFile(const String& path);
void runScriptText(String content);
void startWeb();
bool parentDirectoryExists(const String& path);
String configGetValue(const String& key, const String& fallback = "");
bool configSetValue(const String& key, const String& value);
bool writeWholeFile(const String& path, const String& content);
int pinFromToken(String token);
void saveRelays();
String backupText();
String backupText(bool includeProfiles);
String sensorJson();
String sensorText();
void cmdTimeNet(String args[], int argc);
void cmdMail(String args[], int argc);
void cmdIf(String args[], int argc);
void cmdWhen(String args[], int argc);
void cmdLet(String args[], int argc);
void cmdDefine(String args[], int argc);
void cmdFunction(String args[], int argc);
void cmdCall(String args[], int argc);
String functionPath(String name);
bool functionExists(String name);
void runFunctionInline(String name);
bool ntpSync(bool waitForSync);
void compactLogIfNeeded();
void loadRules();
void processRules();
void loadCrons();
void processCrons();
void loadInputs();
void processInputs();
void processHealthGuard();
bool compareFloat(float left, uint8_t op, float right);
void beginWifiConnect(const String& ssid, const String& password);
String wifiStatusText();
String wifiNetText();
void processWifi();
void startFallbackAp();
void stopFallbackAp();
void saveAliases();
void loadAliases();
void loadHistory();
void cmdHistory(String args[], int argc);
void cmdLogger(String args[], int argc);
void cmdPkg(String args[], int argc);
void cmdOnBoot(String args[], int argc);
void cmdSchedule(String args[], int argc);
void cmdClimate(String args[], int argc);
void cmdPathTool(const String& cmd, String args[], int argc);
void cmdTest(String args[], int argc);
void cmdRepeat(String args[], int argc);
void cmdWatch(String args[], int argc);
void cmdDryRun(String args[], int argc);
void cmdProfile(String args[], int argc);
void cmdBoard(String args[], int argc);
void cmdDiag();
void cmdDiagWifi();
void cmdExport(String args[], int argc);
void cmdStat(String args[], int argc);
void cmdAp(String args[], int argc);
void cmdArm(String args[], int argc);
void cmdIfconfig();
void cmdIp(String args[], int argc);
void cmdCrontab(String args[], int argc);
void cmdSystemctl(String args[], int argc);
void cmdKill(String args[], int argc);
void cmdPgrep(String args[], int argc, bool pidOnly);
String procText(const String& path);
void appendBackupDirRecursive(String& out, const String& dirPath, uint8_t depth);
void sendBackupDirRecursive(const String& dirPath, uint8_t depth);

void addLog(const char* message) {
  dmesgRing[dmesgHead].ms = millis();
  strncpy(dmesgRing[dmesgHead].text, message, DMESG_LEN - 1);
  dmesgRing[dmesgHead].text[DMESG_LEN - 1] = '\0';
  dmesgHead = (dmesgHead + 1) % DMESG_LINES;
}

void addLogLine(const String& message) {
  addLog(message.c_str());
}

void appendPersistentLog(const String& message) {
  if (!fsReady) return;
  LittleFS.mkdir("/var");
  LittleFS.mkdir("/var/log");
  compactLogIfNeeded();
  File file = LittleFS.open(LOG_FILE, "a");
  if (!file) return;
  file.print(millis() / 1000);
  file.print(' ');
  file.println(message);
  file.close();
}

void eventLog(const String& message) {
  addLogLine(message);
  if (configGetValue("log.persist", "off") == "on") appendPersistentLog(message);
}

void compactLogIfNeeded() {
  if (!fsReady || !LittleFS.exists(LOG_FILE)) return;
  File file = LittleFS.open(LOG_FILE, "r");
  if (!file) return;
  size_t size = file.size();
  if (size <= LOG_MAX_BYTES) {
    file.close();
    return;
  }
  size_t keep = min((size_t)LOG_KEEP_BYTES, size);
  file.seek(size - keep, SeekSet);
  String tail = file.readString();
  file.close();
  int firstNewline = tail.indexOf('\n');
  if (firstNewline >= 0 && firstNewline + 1 < (int)tail.length()) {
    tail = tail.substring(firstNewline + 1);
  }
  File out = LittleFS.open(LOG_FILE, "w");
  if (!out) return;
  out.println(F("# log compacted"));
  out.print(tail);
  out.close();
}

String joinArgs(String args[], int argc, int start) {
  String out;
  for (int i = start; i < argc; i++) {
    if (i > start) out += " ";
    out += args[i];
  }
  return out;
}

String safeNameToken(String token) {
  String out;
  for (uint16_t i = 0; i < token.length(); i++) {
    char c = token[i];
    out += isAlphaNumeric(c) ? c : '_';
  }
  return out.length() ? out : "pin";
}

void ensureSystemDirs() {
  if (!fsReady) return;
  LittleFS.mkdir("/etc");
  LittleFS.mkdir("/var");
  LittleFS.mkdir("/var/log");
  LittleFS.mkdir("/home");
  LittleFS.mkdir("/www");
  LittleFS.mkdir(PKG_DIR);
  LittleFS.mkdir(FUNC_DIR);
  LittleFS.mkdir(PROFILE_DIR);
  LittleFS.mkdir(WIFI_PROFILE_DIR);
}

void ensureUnixDefaults() {
  if (!fsReady) return;
  if (!LittleFS.exists(MOTD_FILE)) {
    writeWholeFile(MOTD_FILE, "KernelESP mini UNIX\nType help or help <cmd>.\n");
  }
  if (!LittleFS.exists(DEFAULT_BOOT_SCRIPT)) {
    writeWholeFile(DEFAULT_BOOT_SCRIPT, "# KernelESP boot script\n# Add one command per line.\n");
  }
  if (!configGetValue("boot.script", "").length()) {
    configSetValue("boot.script", DEFAULT_BOOT_SCRIPT);
  }
  LittleFS.mkdir("/help");
  if (!LittleFS.exists("/help/index.txt")) {
    writeWholeFile("/help/index.txt", "KernelESP help\nTopics: relay climate wifi cron web safety\nUse: help <topic> or /help?topic=<topic>\n");
  }
  if (!LittleFS.exists("/help/relay.txt")) {
    writeWholeFile("/help/relay.txt", "relay add light D1 active_low\nrelay on light\nrelay off light\nrelay pulse light 500\n");
  }
  if (!LittleFS.exists("/help/climate.txt")) {
    writeWholeFile("/help/climate.txt", "climate temp fan 38 40\nclimate hum extractor 60 70\n");
  }
  if (!LittleFS.exists("/help/wifi.txt")) {
    writeWholeFile("/help/wifi.txt", "wifi status\nwifi reconnect\nap start\nap status\n");
  }
  if (!LittleFS.exists("/help/web.txt")) {
    writeWholeFile("/help/web.txt", "/diag status and recovery\n/wizard relay/rule/cron forms\n/profiles backup and profiles\n/edit scripts\n");
  }
  if (!LittleFS.exists("/help/safety.txt")) {
    writeWholeFile("/help/safety.txt", "arm\ndisarm\narmed\nfsformat --yes\nrestore <file> --yes\nprofile load <name> --yes\n");
  }
}

void migrateConfigIfNeeded() {
  if (!fsReady) return;
  String schema = configGetValue("system.config_schema", "0");
  if (schema == CONFIG_SCHEMA_VERSION) return;
  if (!configGetValue("board.profile", "").length()) configSetValue("board.profile", "generic");
  if (!configGetValue("web.lockout", "").length()) configSetValue("web.lockout", "on");
  if (!configGetValue("web.lockout.max", "").length()) configSetValue("web.lockout.max", "5");
  if (!configGetValue("web.lockout.ms", "").length()) configSetValue("web.lockout.ms", "300000");
  configSetValue("system.config_schema", CONFIG_SCHEMA_VERSION);
  configSetValue("system.last_firmware", KERNEL_VERSION);
  eventLog("config migrated");
}

void ensureWebAssets() {
  if (!fsReady) return;
  LittleFS.mkdir("/www");
  if (LittleFS.exists("/www/style.css")) return;
  String css = F("body{font-family:sans-serif;margin:0;background:#f5f7fa;color:#15202b}main{max-width:980px;margin:auto;padding:18px}.card,.box{background:white;border:1px solid #d7dee8;border-radius:8px;padding:16px;margin:14px}.btn,button{background:#1167b1;color:white;padding:9px 12px;border-radius:6px;text-decoration:none}pre{background:#0f1720;color:#d7e1ee;padding:14px;overflow:auto}");
  writeWholeFile("/www/style.css", css);
}

String readWholeFile(const String& path) {
  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory()) return "";
  String content = file.readString();
  file.close();
  return content;
}

bool writeWholeFile(const String& path, const String& content) {
  if (!parentDirectoryExists(path)) return false;
  File file = LittleFS.open(path, "w");
  if (!file) return false;
  file.print(content);
  file.close();
  return true;
}

String configGetValue(const String& key, const String& fallback) {
  if (!fsReady) return fallback;
  File file = LittleFS.open(CONF_CONFIG, "r");
  if (!file) return fallback;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#")) continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String k = line.substring(0, eq);
    k.trim();
    if (k == key) {
      String v = line.substring(eq + 1);
      v.trim();
      file.close();
      return v;
    }
  }
  file.close();
  return fallback;
}

bool configSetValue(const String& key, const String& value) {
  if (!fsReady || !key.length() || key.indexOf('=') >= 0) return false;
  if (configGetValue(key, "\x01") == value) return true;
  String out;
  bool found = false;
  File file = LittleFS.open(CONF_CONFIG, "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      String trimmed = line;
      trimmed.trim();
      int eq = trimmed.indexOf('=');
      if (eq > 0 && trimmed.substring(0, eq) == key) {
        out += key + "=" + value + "\n";
        found = true;
      } else if (line.length()) {
        out += line;
        if (!line.endsWith("\n")) out += "\n";
      }
    }
    file.close();
  }
  if (!found) out += key + "=" + value + "\n";
  return writeWholeFile(CONF_CONFIG, out);
}

bool configRemoveValue(const String& key) {
  if (!fsReady) return false;
  String out;
  bool removed = false;
  File file = LittleFS.open(CONF_CONFIG, "r");
  if (!file) return false;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    String trimmed = line;
    trimmed.trim();
    int eq = trimmed.indexOf('=');
    if (eq > 0 && trimmed.substring(0, eq) == key) {
      removed = true;
    } else if (line.length()) {
      out += line;
      if (!line.endsWith("\n")) out += "\n";
    }
  }
  file.close();
  if (removed) writeWholeFile(CONF_CONFIG, out);
  return removed;
}

String normalizePath(const String& raw) {
  if (raw.length() == 0) return cwd;
  String path = raw.startsWith("/") ? raw : cwd + (cwd.endsWith("/") ? "" : "/") + raw;
  path.replace("//", "/");
  if (path.length() > 1 && path.endsWith("/")) path.remove(path.length() - 1);

  String parts[18];
  int count = 0;
  int start = 1;
  int pathLen = path.length();
  while (start <= pathLen && count < 18) {
    int slash = path.indexOf('/', start);
    String part = slash == -1 ? path.substring(start) : path.substring(start, slash);
    start = slash == -1 ? pathLen + 1 : slash + 1;
    if (part.length() == 0 || part == ".") continue;
    if (part == "..") {
      if (count > 0) count--;
    } else {
      parts[count++] = part;
    }
  }

  String result = "/";
  for (int i = 0; i < count; i++) {
    if (i > 0) result += "/";
    result += parts[i];
  }
  return result;
}

String parentPath(const String& path) {
  if (path == "/") return "/";
  int slash = path.lastIndexOf('/');
  if (slash <= 0) return "/";
  return path.substring(0, slash);
}

String basenameOf(const String& path) {
  if (path == "/") return "/";
  int slash = path.lastIndexOf('/');
  return slash == -1 ? path : path.substring(slash + 1);
}

String dirChildPath(const String& dirPath, const String& entryName) {
  if (entryName.startsWith("/")) return entryName;
  String base = basenameOf(entryName);
  return dirPath == "/" ? "/" + base : dirPath + "/" + base;
}

bool ensureFS() {
  if (!fsReady) Serial.println(F("LittleFS not mounted. Try fsformat."));
  return fsReady;
}

bool isDirectory(const String& path) {
  if (path == "/") return true;
  String parent = parentPath(path);
  String base = basenameOf(path);
  Dir dir = LittleFS.openDir(parent);
  while (dir.next()) {
    if (basenameOf(dir.fileName()) == base && dir.isDirectory()) return true;
  }
  return false;
}

bool pathExists(const String& path) {
  if (path == "/proc" || path.startsWith("/proc/")) return true;
  return path == "/" || LittleFS.exists(path) || isDirectory(path);
}

bool parentDirectoryExists(const String& path) {
  String parent = parentPath(path);
  return parent == "/" || isDirectory(parent);
}

String safeName(String name) {
  name.trim();
  String out;
  for (uint16_t i = 0; i < name.length() && out.length() < 32; i++) {
    char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_') {
      out += c;
    }
  }
  return out;
}

String safeFunctionName(String name) {
  name.trim();
  String out;
  for (uint16_t i = 0; i < name.length() && out.length() < 32; i++) {
    char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
      out += c;
    } else if ((c >= '0' && c <= '9') && out.length()) {
      out += c;
    }
  }
  return out;
}

String keyValueFromText(const String& text, const String& key, const String& fallback = "") {
  int start = 0;
  while (start < (int)text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = text.substring(start, end);
    line.trim();
    if (!line.startsWith("#")) {
      int eq = line.indexOf('=');
      if (eq > 0) {
        String k = line.substring(0, eq);
        k.trim();
        if (k == key) {
          String v = line.substring(eq + 1);
          v.trim();
          return v;
        }
      }
    }
    start = end + 1;
  }
  return fallback;
}

bool resolvePin(const String& token, int& pin) {
  pin = pinFromToken(token);
  if (pin < 0 || pin > 16) {
    Serial.println(F("bad pin"));
    return false;
  }
  if (pin >= 6 && pin <= 11) {
    Serial.println(F("bad pin: GPIO6-GPIO11 are connected to flash"));
    return false;
  }
  if (pin == 0 || pin == 1 || pin == 2 || pin == 3 || pin == 15) {
    Serial.println(F("warning: boot/serial pin; use carefully"));
  }
  return true;
}

int splitArgs(String line, String args[], int maxArgs) {
  int count = 0;
  String current;
  bool quoted = false;
  char quoteChar = '\0';

  line.trim();
  for (uint16_t i = 0; i < line.length(); i++) {
    char c = line[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
    } else if (isSpace(c) && !quoted) {
      if (current.length()) {
        if (count < maxArgs - 1) {
          args[count++] = current;
          current = "";
        } else {
          String rest = line.substring(i + 1);
          rest.trim();
          if (rest.length()) current += " " + rest;
          break;
        }
      }
    } else {
      current += c;
    }
  }
  if (current.length() && count < maxArgs) args[count++] = current;
  return count;
}

String stripCLineComment(String line) {
  bool quoted = false;
  char quoteChar = '\0';
  for (uint16_t i = 0; i + 1 < line.length(); i++) {
    char c = line[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
    } else if (!quoted && c == '/' && line[i + 1] == '/' && (i == 0 || isSpace(line[i - 1]))) {
      return line.substring(0, i);
    }
  }
  return line;
}

int braceDepthDelta(const String& line) {
  bool quoted = false;
  char quoteChar = '\0';
  int delta = 0;
  for (uint16_t i = 0; i < line.length(); i++) {
    char c = line[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
    } else if (!quoted && c == '{') {
      delta++;
    } else if (!quoted && c == '}') {
      delta--;
    }
  }
  return delta;
}

int pinFromToken(String token) {
  token.toUpperCase();
  for (uint8_t i = 0; i < sizeof(PIN_ALIASES) / sizeof(PIN_ALIASES[0]); i++) {
    if (token == PIN_ALIASES[i].name) return PIN_ALIASES[i].gpio;
  }
  if (token.startsWith("GPIO")) token = token.substring(4);
  for (uint8_t i = 0; i < token.length(); i++) {
    if (!isDigit(token[i])) return -1;
  }
  return token.length() ? token.toInt() : -1;
}

uint8_t i2cRead8(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return 0;
  Wire.requestFrom((int)addr, 1);
  return Wire.available() ? Wire.read() : 0;
}

uint16_t i2cRead16LE(uint8_t addr, uint8_t reg) {
  uint8_t lsb = i2cRead8(addr, reg);
  uint8_t msb = i2cRead8(addr, reg + 1);
  return (uint16_t)msb << 8 | lsb;
}

int16_t i2cReadS16LE(uint8_t addr, uint8_t reg) {
  return (int16_t)i2cRead16LE(addr, reg);
}

bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

uint8_t parseHexOrDec(String token) {
  token.trim();
  token.toLowerCase();
  if (token.startsWith("0x")) return (uint8_t)strtoul(token.c_str(), nullptr, 16);
  return (uint8_t)token.toInt();
}

void addHistory(const String& line) {
  if (!line.length()) return;
  if (historyCount < MAX_HISTORY) {
    history[historyCount++] = line;
  } else {
    for (uint8_t i = 1; i < MAX_HISTORY; i++) history[i - 1] = history[i];
    history[MAX_HISTORY - 1] = line;
  }
}

void saveHistory() {
  if (!ensureFS()) return;
  String out;
  for (uint8_t i = 0; i < historyCount; i++) out += history[i] + "\n";
  Serial.println(writeWholeFile(HISTORY_FILE, out) ? F("OK") : F("history: save failed"));
}

void loadHistory() {
  if (!fsReady || !LittleFS.exists(HISTORY_FILE)) return;
  File file = LittleFS.open(HISTORY_FILE, "r");
  if (!file) return;
  historyCount = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length()) addHistory(line);
  }
  file.close();
}

void cmdHistory(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (sub == "list") {
    for (uint8_t i = 0; i < historyCount; i++) {
      Serial.print(i + 1); Serial.print(F("  ")); Serial.println(history[i]);
    }
  } else if (sub == "clear") {
    historyCount = 0;
    Serial.println(F("OK"));
  } else if (sub == "save") {
    saveHistory();
  } else if (sub == "load") {
    loadHistory();
    Serial.println(F("OK"));
  } else {
    Serial.println(F("usage: history [list|clear|save|load]"));
  }
}

void saveAliases() {
  if (!ensureFS()) return;
  String out;
  for (uint8_t i = 0; i < MAX_ALIASES; i++) {
    if (!aliases[i].active) continue;
    out += aliases[i].name + "=" + aliases[i].command + "\n";
  }
  Serial.println(writeWholeFile(CONF_ALIASES, out) ? F("OK") : F("alias: save failed"));
}

void loadAliases() {
  if (!fsReady || !LittleFS.exists(CONF_ALIASES)) return;
  File file = LittleFS.open(CONF_ALIASES, "r");
  if (!file) return;
  for (uint8_t i = 0; i < MAX_ALIASES; i++) aliases[i] = AliasEntry();
  uint8_t idx = 0;
  while (file.available() && idx < MAX_ALIASES) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#")) continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    aliases[idx].name = line.substring(0, eq);
    aliases[idx].command = line.substring(eq + 1);
    aliases[idx].name.trim();
    aliases[idx].command.trim();
    aliases[idx].active = aliases[idx].name.length() && aliases[idx].command.length();
    if (aliases[idx].active) idx++;
  }
  file.close();
}

bool isKnownCommand(const String& cmd) {
  static const char* commands[] = {
    "help", "man", "clear", "echo", "history", "alias", "unalias", "env", "printenv", "set", "setenv", "unset", "export",
    "arm", "disarm", "armed",
    "true", "false", "test", "[", "basename", "dirname", "repeat", "watch",
    "if", "when", "let", "var", "define", "undef", "function", "call",
    "id", "groups", "who", "w", "sync",
    "uname", "uptime", "free", "heap", "mem", "ps", "top", "pgrep", "pidof", "kill", "dmesg", "reboot", "resetreason",
    "chip", "flash", "sysinfo", "pwd", "cd", "ls", "cat", "head", "tail", "grep", "find", "stat",
    "wc", "du", "touch", "write", "append", "rm", "mkdir", "rmdir", "cp", "mv", "df",
    "fsformat", "mount", "which", "whoami", "hostname", "jobs", "motd", "state", "scene", "input",
    "health", "diag", "service", "restore", "board", "pins", "pinmode",
    "gpio", "read", "writepin", "toggle", "pwm", "adc", "wifi", "ifconfig", "ip", "relay", "sensor", "pcf", "mcp", "rule",
    "cron", "crontab", "timer", "date", "i2c", "sh", "source", "run", "boot", "onboot", "safe", "config", "log", "logger",
    "pkg", "profile", "dryrun", "schedule", "climate", "web", "ap", "sleep", "time", "ntp", "mail", "ping", "httpget",
    "backup", "journalctl", "service", "systemctl", "version"
  };
  for (uint8_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
    if (cmd == commands[i]) return true;
  }
  return false;
}

void printHelpTopic(String topic) {
  topic.toLowerCase();
  if (topic == "ls") Serial.println(F("ls [path] - list files"));
  else if (topic == "cat") Serial.println(F("cat <file> - print a file"));
  else if (topic == "head") Serial.println(F("head [-n lines] <file> - print first lines"));
  else if (topic == "tail") Serial.println(F("tail [-n lines] <file> - print last lines"));
  else if (topic == "grep") Serial.println(F("grep <text> <file> - print matching lines; in pipes: cmd | grep <text>"));
  else if (topic == "find") Serial.println(F("find [dir] [text] - list files, optionally matching text"));
  else if (topic == "wc") Serial.println(F("wc <file> - count lines words bytes"));
  else if (topic == "du") Serial.println(F("du [path] - show file/directory byte usage"));
  else if (topic == "cron") Serial.println(F("cron add HH:MM|daily|dow|date ...; cron list|rm|clear"));
  else if (topic == "rule") Serial.println(F("rule add temp|hum|press =|!=|<|>|<=|>=|range ...; cooldown|every|list"));
  else if (topic == "timer") Serial.println(F("timer every|once <ms> <command>; timer list|rm|clear"));
  else if (topic == "relay") Serial.println(F("relay add|rm|on|off|toggle|pulse|status|boot"));
  else if (topic == "scene") Serial.println(F("scene add <name> <cmd[;cmd]>; scene run|list|show|rm|clear"));
  else if (topic == "state") Serial.println(F("state set|get|rm|list|clear persistent key values"));
  else if (topic == "input") Serial.println(F("input add <name> <pin> pullup|float; input on <name> high|low|change <cmd>"));
  else if (topic == "if") Serial.println(F("if (<expr>) { cmd; cmd } else { cmd }; ops: == != < > <= >= && || !"));
  else if (topic == "when") Serial.println(F("when input|pin <name|pin> high|low|pulse if (<expr>) <cmd>"));
  else if (topic == "let" || topic == "var") Serial.println(F("let name = value - persistent variable stored in state"));
  else if (topic == "define") Serial.println(F("define NAME value; undef NAME - constants for expressions"));
  else if (topic == "function") Serial.println(F("function name { cmd; cmd }; call name - persistent mini script"));
  else if (topic == "health") Serial.println(F("health [guard <min_heap>|off] - system summary and heap guard"));
  else if (topic == "diag") Serial.println(F("diag - read-only diagnostic bundle for support"));
  else if (topic == "board") Serial.println(F("board list|show|pins|use <profile> - board profile and pin guidance"));
  else if (topic == "service") Serial.println(F("service [name] status|start|stop|restart for web, ntp, sensor"));
  else if (topic == "restore") Serial.println(F("restore <backup-file> --yes - restore files from KernelESP backup"));
  else if (topic == "sensor") Serial.println(F("sensor begin|scan|read|status|save|autostart"));
  else if (topic == "pcf") Serial.println(F("pcf read|write <addr> [value] for PCF8574 GPIO expanders"));
  else if (topic == "mcp") Serial.println(F("mcp init|read|write <addr> ... for MCP23017 GPIO expanders"));
  else if (topic == "boot") Serial.println(F("boot show|set <script>|run [script]; default /etc/boot.sh"));
  else if (topic == "onboot") Serial.println(F("onboot list|add <cmd>|rm <n>|clear - edit /etc/boot.sh"));
  else if (topic == "pkg") Serial.println(F("pkg list|add|run|show|rm - script packages in /pkg"));
  else if (topic == "profile") Serial.println(F("profile list|save|load <name> --yes|show|rm - manual config snapshots"));
  else if (topic == "dryrun") Serial.println(F("dryrun on|off|status - simulate GPIO/relay writes"));
  else if (topic == "arm") Serial.println(F("arm|disarm|armed - enable or pause automations"));
  else if (topic == "ap") Serial.println(F("ap start|stop|status - fallback setup access point"));
  else if (topic == "schedule") Serial.println(F("schedule <relay> <onHH:MM> <offHH:MM> - add daily relay cron"));
  else if (topic == "climate") Serial.println(F("climate temp|hum <relay> <low> <high> - add hysteresis rule"));
  else if (topic == "log") Serial.println(F("log show|tail|head|clear|compact|flash on|off; logger <msg>"));
  else if (topic == "jobs") Serial.println(F("jobs - list timers, rules and cron entries"));
  else if (topic == "kill") Serial.println(F("kill <pid> - remove timer/rule/cron pseudo process"));
  else if (topic == "pgrep") Serial.println(F("pgrep [-a] <text>; pidof <text> - find timer/rule/cron commands"));
  else if (topic == "ifconfig" || topic == "ip") Serial.println(F("ifconfig | ip addr - show Wi-Fi interface"));
  else if (topic == "crontab") Serial.println(F("crontab -l|-r|add <cron args> - wrapper for cron"));
  else if (topic == "systemctl") Serial.println(F("systemctl status|start|stop|restart <service> - wrapper for service"));
  else if (topic == "which") Serial.println(F("which <cmd> - show if command is built in"));
  else if (topic == "mount") Serial.println(F("mount - show mounted pseudo filesystems"));
  else if (topic == "motd") Serial.println(F("motd [text] - show or replace /etc/motd"));
  else if (topic == "wifi") Serial.println(F("wifi scan|status|connect|save|forget|dhcp|static|net|timeout|channel|phy|power|recover|sdkreset|wait|ip|mac|hostname"));
  else if (topic == "ntp" || topic == "time") Serial.println(F("ntp kick|sync|status|auto|server|tz"));
  else if (topic == "mail" || topic == "email") Serial.println(F("mail status|config|send|test|health - plain SMTP via LAN relay"));
  else if (topic == "pipe" || topic == "pipes") Serial.println(F("Pipes: cmd | grep text | head -n 5 | tail -n 5 | wc [-l|-w|-c] | tee /path/file"));
  else Serial.println(F("help: no detailed help for that command"));
}

void cmdHelp(String args[], int argc) {
  if (argc >= 2) {
    printHelpTopic(args[1]);
    return;
  }
  Serial.println(F("Commands:"));
  Serial.println(F("  help clear echo history alias unalias env printenv set setenv unset true false test if when let var define undef function call arm disarm armed"));
  Serial.println(F("  id groups who w sync uname uptime free heap mem ps top pgrep pidof kill dmesg reboot resetreason chip flash sysinfo"));
  Serial.println(F("  pwd cd ls cat head tail grep find wc du stat basename dirname touch write append rm mkdir rmdir cp mv df fsformat"));
  Serial.println(F("  mount which whoami hostname jobs motd health diag service board"));
  Serial.println(F("  state scene input restore"));
  Serial.println(F("  pins pinmode gpio read writepin toggle pwm adc"));
  Serial.println(F("  wifi scan status connect save forget dhcp static net timeout channel phy power recover sdkreset autoconnect reconnect wait disconnect ip mac mode hostname ifconfig ip"));
  Serial.println(F("  relay add rm on off toggle pulse status boot save load"));
  Serial.println(F("  sensor begin scan read status save autostart pcf mcp"));
  Serial.println(F("  rule add/list/rm/clear/every/cooldown/save/load"));
  Serial.println(F("  cron add daily|dow|date list rm clear every save load crontab"));
  Serial.println(F("  date i2c scan sh source run boot onboot safe config log logger journalctl pkg profile dryrun ap repeat watch"));
  Serial.println(F("  export schedule climate timer web systemctl sleep time ntp ping httpget backup version"));
  Serial.println(F("  pipes: cmd | grep text | head -n 5 | tail -n 5 | wc [-l|-w|-c] | tee /path/file"));
}

void cmdLs(String args[], int argc) {
  if (!ensureFS()) return;
  String path = argc > 1 ? normalizePath(args[1]) : cwd;
  if (path == "/proc") {
    Serial.println(F("meminfo"));
    Serial.println(F("uptime"));
    Serial.println(F("wifi"));
    Serial.println(F("relays"));
    Serial.println(F("version"));
    Serial.println(F("filesystems"));
    Serial.println(F("flash"));
    return;
  }
  if (!pathExists(path)) {
    Serial.println(F("ls: not found"));
    return;
  }
  if (!isDirectory(path)) {
    File file = LittleFS.open(path, "r");
    if (!file) { Serial.println(F("ls: cannot open file")); return; }
    Serial.print(basenameOf(path));
    Serial.print('\t');
    Serial.print(file.size());
    Serial.println(F("B"));
    file.close();
    return;
  }
  Dir dir = LittleFS.openDir(path);
  bool empty = true;
  while (dir.next()) {
    empty = false;
    Serial.print(basenameOf(dir.fileName()));
    if (dir.isDirectory()) Serial.print('/');
    else {
      Serial.print('\t');
      Serial.print(dir.fileSize());
      Serial.print(F("B"));
    }
    Serial.println();
  }
  if (empty) Serial.println(F("(empty)"));
}

void cmdCat(String args[], int argc) {
  if (!ensureFS()) return;
  if (argc < 2) { Serial.println(F("usage: cat <file>")); return; }
  String path = normalizePath(args[1]);
  if (path.startsWith("/proc/")) {
    String out = procText(path);
    if (!out.length()) Serial.println(F("cat: cannot open file"));
    else Serial.print(out);
    return;
  }
  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory()) { Serial.println(F("cat: cannot open file")); return; }
  while (file.available()) Serial.write(file.read());
  Serial.println();
  file.close();
}

int parseLineLimit(String args[], int argc, int& pathArg, int fallback) {
  pathArg = 1;
  int limit = fallback;
  if (argc >= 4 && args[1] == "-n") {
    limit = args[2].toInt();
    pathArg = 3;
  } else if (argc >= 3 && args[1].startsWith("-")) {
    limit = args[1].substring(1).toInt();
    pathArg = 2;
  }
  if (limit < 1) limit = fallback;
  if (limit > 40) limit = 40;
  return limit;
}

void cmdHead(String args[], int argc) {
  if (!ensureFS()) return;
  int pathArg;
  int lines = parseLineLimit(args, argc, pathArg, 10);
  if (argc <= pathArg) { Serial.println(F("usage: head [-n lines] <file>")); return; }
  File file = LittleFS.open(normalizePath(args[pathArg]), "r");
  if (!file || file.isDirectory()) { Serial.println(F("head: cannot open file")); return; }
  int printed = 0;
  while (file.available() && printed < lines) {
    String line = file.readStringUntil('\n');
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    Serial.println(line);
    printed++;
    yield();
  }
  file.close();
}

void cmdTail(String args[], int argc) {
  if (!ensureFS()) return;
  int pathArg;
  int lines = parseLineLimit(args, argc, pathArg, 10);
  if (argc <= pathArg) { Serial.println(F("usage: tail [-n lines] <file>")); return; }
  File file = LittleFS.open(normalizePath(args[pathArg]), "r");
  if (!file || file.isDirectory()) { Serial.println(F("tail: cannot open file")); return; }
  String ring[40];
  int count = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    ring[count % lines] = line;
    count++;
    yield();
  }
  file.close();
  int start = count > lines ? count - lines : 0;
  for (int i = start; i < count; i++) Serial.println(ring[i % lines]);
}

void cmdGrep(String args[], int argc) {
  if (!ensureFS()) return;
  if (argc < 3) { Serial.println(F("usage: grep <text> <file>")); return; }
  String pattern = args[1];
  File file = LittleFS.open(normalizePath(args[2]), "r");
  if (!file || file.isDirectory()) { Serial.println(F("grep: cannot open file")); return; }
  uint16_t lineNo = 0;
  bool any = false;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    lineNo++;
    if (line.indexOf(pattern) >= 0) {
      any = true;
      Serial.print(lineNo);
      Serial.print(':');
      Serial.println(line);
    }
    yield();
  }
  file.close();
  if (!any) Serial.println(F("(no matches)"));
}

void cmdWc(String args[], int argc) {
  if (!ensureFS()) return;
  if (argc < 2) { Serial.println(F("usage: wc <file>")); return; }
  File file = LittleFS.open(normalizePath(args[1]), "r");
  if (!file || file.isDirectory()) { Serial.println(F("wc: cannot open file")); return; }
  uint32_t bytes = 0, lines = 0, words = 0;
  bool inWord = false;
  while (file.available()) {
    char c = file.read();
    bytes++;
    if (c == '\n') lines++;
    if (isSpace(c)) inWord = false;
    else if (!inWord) {
      words++;
      inWord = true;
    }
    yield();
  }
  file.close();
  Serial.print(lines); Serial.print(' ');
  Serial.print(words); Serial.print(' ');
  Serial.print(bytes); Serial.print(' ');
  Serial.println(args[1]);
}

uint32_t duPath(const String& path, uint8_t depth) {
  if (depth > 6) return 0;
  if (!isDirectory(path)) {
    File file = LittleFS.open(path, "r");
    if (!file) return 0;
    uint32_t size = file.size();
    file.close();
    return size;
  }
  uint32_t total = 0;
  Dir dir = LittleFS.openDir(path);
  while (dir.next()) {
    String child = dirChildPath(path, dir.fileName());
    if (dir.isDirectory()) total += duPath(child, depth + 1);
    else total += dir.fileSize();
    yield();
  }
  return total;
}

void cmdDu(String args[], int argc) {
  if (!ensureFS()) return;
  String path = argc >= 2 ? normalizePath(args[1]) : cwd;
  if (!pathExists(path)) { Serial.println(F("du: not found")); return; }
  Serial.print(duPath(path, 0));
  Serial.print('\t');
  Serial.println(path);
}

bool findPath(const String& path, const String& pattern, uint8_t depth) {
  if (depth > 6) return false;
  bool any = false;
  Dir dir = LittleFS.openDir(path);
  while (dir.next()) {
    String child = dirChildPath(path, dir.fileName());
    String name = basenameOf(child);
    bool match = !pattern.length() || child.indexOf(pattern) >= 0 || name.indexOf(pattern) >= 0;
    if (match) {
      any = true;
      Serial.print(child);
      if (dir.isDirectory()) Serial.print('/');
      Serial.println();
    }
    if (dir.isDirectory() && findPath(child, pattern, depth + 1)) any = true;
    yield();
  }
  return any;
}

void cmdFind(String args[], int argc) {
  if (!ensureFS()) return;
  String start = argc >= 2 ? normalizePath(args[1]) : cwd;
  String pattern = argc >= 3 ? args[2] : "";
  if (!isDirectory(start)) { Serial.println(F("find: not a directory")); return; }
  if (!findPath(start, pattern, 0)) Serial.println(F("(no matches)"));
}

void cmdMotd(String args[], int argc) {
  if (!ensureFS()) return;
  if (argc >= 2) {
    Serial.println(writeWholeFile(MOTD_FILE, joinArgs(args, argc, 1) + "\n") ? F("OK") : F("motd: write failed"));
    return;
  }
  String motd = readWholeFile(MOTD_FILE);
  Serial.print(motd.length() ? motd : "(empty)\n");
}

void writeFileCommand(String args[], int argc, bool append) {
  if (!ensureFS()) return;
  if (argc < 3) {
    Serial.println(append ? F("usage: append <file> <text>") : F("usage: write <file> <text>"));
    return;
  }
  String path = normalizePath(args[1]);
  if (!parentDirectoryExists(path)) { Serial.println(F("write: parent directory not found")); return; }
  if (isDirectory(path)) { Serial.println(F("write: target is a directory")); return; }
  File file = LittleFS.open(path, append ? "a" : "w");
  if (!file) { Serial.println(F("write: cannot open file")); return; }
  file.print(joinArgs(args, argc, 2));
  if (append) file.println();
  file.close();
  Serial.println(F("OK"));
}

void cmdDf() {
  if (!ensureFS()) return;
  FSInfo info;
  LittleFS.info(info);
  Serial.print(F("total: ")); Serial.println(info.totalBytes);
  Serial.print(F("used:  ")); Serial.println(info.usedBytes);
  Serial.print(F("free:  ")); Serial.println(info.totalBytes - info.usedBytes);
  Serial.print(F("block: ")); Serial.println(info.blockSize);
  Serial.print(F("page:  ")); Serial.println(info.pageSize);
}

void cmdMount() {
  Serial.println(F("rootfs on / type littlefs (rw)"));
  Serial.println(F("proc on /proc type kernelesp (ro)"));
  Serial.println(F("sysfs on /sys type esp8266 (ro)"));
}

void cmdWhich(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: which <command>")); return; }
  String cmd = args[1];
  cmd.toLowerCase();
  if (isKnownCommand(cmd)) {
    Serial.print(F("builtin:"));
    Serial.println(cmd);
  } else {
    Serial.println(F("not found"));
  }
}

void cmdJobs() {
  Serial.println(F("[timers]"));
  bool any = false;
  for (uint8_t i = 0; i < MAX_TIMERS; i++) {
    if (!timers[i].active) continue;
    any = true;
    Serial.print(timers[i].id);
    Serial.print(timers[i].repeat ? F(" every ") : F(" once "));
    Serial.print(timers[i].intervalMs);
    Serial.print(F("ms -> "));
    Serial.println(timers[i].command);
  }
  if (!any) Serial.println(F("(none)"));
  Serial.println(F("[rules]"));
  Serial.print(rulesText());
  Serial.println(F("[cron]"));
  Serial.print(cronsText());
}

String kvGetFile(const String& filePath, const String& key, const String& fallback) {
  if (!fsReady) return fallback;
  File file = LittleFS.open(filePath, "r");
  if (!file) return fallback;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#")) continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String k = line.substring(0, eq);
    k.trim();
    if (k == key) {
      String v = line.substring(eq + 1);
      v.trim();
      file.close();
      return v;
    }
  }
  file.close();
  return fallback;
}

bool kvGetMaybe(const String& filePath, const String& key, String& value) {
  if (!fsReady || !key.length()) return false;
  File file = LittleFS.open(filePath, "r");
  if (!file) return false;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#")) continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String k = line.substring(0, eq);
    k.trim();
    if (k == key) {
      value = line.substring(eq + 1);
      value.trim();
      file.close();
      return true;
    }
  }
  file.close();
  return false;
}

bool kvSetFile(const String& filePath, const String& key, const String& value) {
  if (!ensureFS() || !key.length() || key.indexOf('=') >= 0) return false;
  String out;
  bool found = false;
  File file = LittleFS.open(filePath, "r");
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      String trimmed = line;
      trimmed.trim();
      int eq = trimmed.indexOf('=');
      if (eq > 0 && trimmed.substring(0, eq) == key) {
        out += key + "=" + value + "\n";
        found = true;
      } else if (line.length()) {
        out += line;
        if (!line.endsWith("\n")) out += "\n";
      }
    }
    file.close();
  }
  if (!found) out += key + "=" + value + "\n";
  return writeWholeFile(filePath, out);
}

bool kvRemoveFile(const String& filePath, const String& key) {
  if (!ensureFS()) return false;
  String out;
  bool removed = false;
  File file = LittleFS.open(filePath, "r");
  if (!file) return false;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    String trimmed = line;
    trimmed.trim();
    int eq = trimmed.indexOf('=');
    if (eq > 0 && trimmed.substring(0, eq) == key) removed = true;
    else if (line.length()) {
      out += line;
      if (!line.endsWith("\n")) out += "\n";
    }
  }
  file.close();
  if (removed) writeWholeFile(filePath, out);
  return removed;
}

void cmdState(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (sub == "list") {
    String out = readWholeFile(CONF_STATE);
    Serial.print(out.length() ? out : "(empty)\n");
  } else if (sub == "get") {
    if (argc < 3) { Serial.println(F("usage: state get <key>")); return; }
    Serial.println(kvGetFile(CONF_STATE, args[2], ""));
  } else if (sub == "set") {
    if (argc < 4) { Serial.println(F("usage: state set <key> <value>")); return; }
    Serial.println(kvSetFile(CONF_STATE, args[2], joinArgs(args, argc, 3)) ? F("OK") : F("state: set failed"));
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: state rm <key>")); return; }
    Serial.println(kvRemoveFile(CONF_STATE, args[2]) ? F("OK") : F("state: not found"));
  } else if (sub == "clear") {
    Serial.println(writeWholeFile(CONF_STATE, "") ? F("OK") : F("state: clear failed"));
  } else {
    Serial.println(F("usage: state list|get|set|rm|clear"));
  }
}

String sceneGet(const String& name) {
  return kvGetFile(CONF_SCENES, name, "");
}

void cmdScene(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (sub == "list") {
    String out = readWholeFile(CONF_SCENES);
    Serial.print(out.length() ? out : "(no scenes)\n");
  } else if (sub == "add" || sub == "set") {
    if (argc < 4) { Serial.println(F("usage: scene add <name> <command[; command...]>")); return; }
    Serial.println(kvSetFile(CONF_SCENES, args[2], joinArgs(args, argc, 3)) ? F("OK") : F("scene: save failed"));
  } else if (sub == "run") {
    if (argc < 3) { Serial.println(F("usage: scene run <name>")); return; }
    String body = sceneGet(args[2]);
    if (!body.length()) { Serial.println(F("scene: not found")); return; }
    body.replace(";", "\n");
    runScriptText(body);
  } else if (sub == "show") {
    if (argc < 3) { Serial.println(F("usage: scene show <name>")); return; }
    String body = sceneGet(args[2]);
    Serial.println(body.length() ? body : "(not found)");
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: scene rm <name>")); return; }
    Serial.println(kvRemoveFile(CONF_SCENES, args[2]) ? F("OK") : F("scene: not found"));
  } else if (sub == "clear") {
    Serial.println(writeWholeFile(CONF_SCENES, "") ? F("OK") : F("scene: clear failed"));
  } else {
    Serial.println(F("usage: scene list|add|show|run|rm|clear"));
  }
}

int findInput(const String& name) {
  for (uint8_t i = 0; i < MAX_INPUTS; i++) {
    if (inputs[i].active && inputs[i].name == name) return i;
  }
  return -1;
}

void saveInputs() {
  if (!ensureFS()) return;
  String out;
  for (uint8_t i = 0; i < MAX_INPUTS; i++) {
    if (!inputs[i].active) continue;
    out += inputs[i].name + ",";
    out += String(inputs[i].pin) + ",";
    out += (inputs[i].pullup ? "pullup" : "float");
    out += ",";
    out += String(inputs[i].debounceMs) + ",";
    out += inputs[i].highCommand + ",";
    out += inputs[i].lowCommand + ",";
    out += inputs[i].changeCommand + "\n";
  }
  writeWholeFile(CONF_INPUTS, out);
}

void loadInputs() {
  for (uint8_t i = 0; i < MAX_INPUTS; i++) inputs[i] = InputWatcher();
  if (!fsReady || !LittleFS.exists(CONF_INPUTS)) return;
  File file = LittleFS.open(CONF_INPUTS, "r");
  if (!file) return;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#")) continue;
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    int p4 = line.indexOf(',', p3 + 1);
    int p5 = line.indexOf(',', p4 + 1);
    int p6 = line.indexOf(',', p5 + 1);
    if (p1 <= 0 || p2 <= p1 || p3 <= p2 || p4 <= p3 || p5 <= p4 || p6 <= p5) continue;
    for (uint8_t i = 0; i < MAX_INPUTS; i++) {
      if (inputs[i].active) continue;
      inputs[i].name = line.substring(0, p1);
      inputs[i].pin = line.substring(p1 + 1, p2).toInt();
      inputs[i].pullup = line.substring(p2 + 1, p3) != "float";
      inputs[i].debounceMs = max(5UL, (unsigned long)line.substring(p3 + 1, p4).toInt());
      inputs[i].highCommand = line.substring(p4 + 1, p5);
      inputs[i].lowCommand = line.substring(p5 + 1, p6);
      inputs[i].changeCommand = line.substring(p6 + 1);
      pinMode(inputs[i].pin, inputs[i].pullup ? INPUT_PULLUP : INPUT);
      bool state = digitalRead(inputs[i].pin);
      inputs[i].stableState = state;
      inputs[i].lastRead = state;
      inputs[i].lastChangeMs = millis();
      inputs[i].active = inputs[i].name.length() > 0 && inputs[i].pin >= 0 && inputs[i].pin <= 16;
      break;
    }
  }
  file.close();
}

String inputsText() {
  String out;
  bool any = false;
  for (uint8_t i = 0; i < MAX_INPUTS; i++) {
    if (!inputs[i].active) continue;
    any = true;
    out += inputs[i].name + " GPIO" + String(inputs[i].pin);
    out += inputs[i].pullup ? " pullup " : " float ";
    out += inputs[i].stableState ? "HIGH" : "LOW";
    out += " debounce=" + String(inputs[i].debounceMs) + "ms\n";
    if (inputs[i].highCommand.length()) out += "  high -> " + inputs[i].highCommand + "\n";
    if (inputs[i].lowCommand.length()) out += "  low -> " + inputs[i].lowCommand + "\n";
    if (inputs[i].changeCommand.length()) out += "  change -> " + inputs[i].changeCommand + "\n";
  }
  if (!any) out += "(no inputs)\n";
  return out;
}

void processInputs() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_INPUTS; i++) {
    if (!inputs[i].active) continue;
    bool reading = digitalRead(inputs[i].pin);
    if (reading != inputs[i].lastRead) {
      inputs[i].lastRead = reading;
      inputs[i].lastChangeMs = now;
    }
    if (reading != inputs[i].stableState && (now - inputs[i].lastChangeMs) >= inputs[i].debounceMs) {
      inputs[i].stableState = reading;
      Serial.print(F("[input "));
      Serial.print(inputs[i].name);
      Serial.println(reading ? F(" high]") : F(" low]"));
      if (inputs[i].changeCommand.length()) executeLine(inputs[i].changeCommand);
      if (reading && inputs[i].highCommand.length()) executeLine(inputs[i].highCommand);
      if (!reading && inputs[i].lowCommand.length()) executeLine(inputs[i].lowCommand);
      yield();
    }
  }
}

void cmdInput(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (sub == "list" || sub == "status") {
    Serial.print(inputsText());
  } else if (sub == "add") {
    if (argc < 5) { Serial.println(F("usage: input add <name> <pin> pullup|float [debounce_ms]")); return; }
    if (findInput(args[2]) >= 0) { Serial.println(F("input: already exists")); return; }
    int pin;
    if (!resolvePin(args[3], pin)) return;
    String mode = args[4]; mode.toLowerCase();
    bool pullup = mode != "float";
    unsigned long debounce = argc >= 6 ? max(5UL, (unsigned long)args[5].toInt()) : 50UL;
    for (uint8_t i = 0; i < MAX_INPUTS; i++) {
      if (inputs[i].active) continue;
      inputs[i] = InputWatcher();
      inputs[i].name = args[2];
      inputs[i].pin = pin;
      inputs[i].pullup = pullup;
      inputs[i].debounceMs = debounce;
      pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
      inputs[i].stableState = digitalRead(pin);
      inputs[i].lastRead = inputs[i].stableState;
      inputs[i].lastChangeMs = millis();
      inputs[i].active = true;
      saveInputs();
      Serial.println(F("OK"));
      return;
    }
    Serial.println(F("input: table full"));
  } else if (sub == "on") {
    if (argc < 5) { Serial.println(F("usage: input on <name> high|low|change <command>")); return; }
    int idx = findInput(args[2]);
    if (idx < 0) { Serial.println(F("input: not found")); return; }
    String edge = args[3]; edge.toLowerCase();
    String command = joinArgs(args, argc, 4);
    if (edge == "high" || edge == "on") inputs[idx].highCommand = command;
    else if (edge == "low" || edge == "off") inputs[idx].lowCommand = command;
    else if (edge == "change") inputs[idx].changeCommand = command;
    else { Serial.println(F("input: edge must be high, low, or change")); return; }
    saveInputs();
    Serial.println(F("OK"));
  } else if (sub == "read") {
    if (argc < 3) { Serial.println(F("usage: input read <name>")); return; }
    int idx = findInput(args[2]);
    if (idx < 0) { Serial.println(F("input: not found")); return; }
    Serial.println(digitalRead(inputs[idx].pin) ? F("HIGH") : F("LOW"));
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: input rm <name>")); return; }
    int idx = findInput(args[2]);
    if (idx < 0) { Serial.println(F("input: not found")); return; }
    inputs[idx] = InputWatcher();
    saveInputs();
    Serial.println(F("OK"));
  } else if (sub == "clear") {
    for (uint8_t i = 0; i < MAX_INPUTS; i++) inputs[i] = InputWatcher();
    saveInputs();
    Serial.println(F("OK"));
  } else if (sub == "save") {
    saveInputs();
    Serial.println(F("OK"));
  } else if (sub == "load") {
    loadInputs();
    Serial.println(F("OK"));
  } else {
    Serial.println(F("usage: input add|on|read|list|rm|clear|save|load"));
  }
}

void cmdPins() {
  Serial.println(F("Safe-ish NodeMCU pins: D1 D2 D5 D6 D7"));
  Serial.println(F("Boot/serial pins: D0 D3 D4 D8 RX TX LED"));
  for (uint8_t i = 0; i < sizeof(PIN_ALIASES) / sizeof(PIN_ALIASES[0]); i++) {
    Serial.print(PIN_ALIASES[i].name);
    Serial.print(F("=GPIO"));
    Serial.print(PIN_ALIASES[i].gpio);
    if (PIN_ALIASES[i].risky) Serial.print(F(" *"));
    Serial.println();
  }
}

bool parseIpAddress(String text, IPAddress& ip) {
  text.trim();
  return text.length() && ip.fromString(text);
}

bool isReconnectArg(const String& arg) {
  return arg == "reconnect" || arg == "--reconnect" || arg == "apply";
}

unsigned long wifiConnectTimeoutMs() {
  unsigned long ms = (unsigned long)configGetValue("wifi.timeout.ms", String(DEFAULT_WIFI_CONNECT_TIMEOUT_MS)).toInt();
  if (ms < 10000UL) ms = 10000UL;
  if (ms > 120000UL) ms = 120000UL;
  return ms;
}

int wifiConnectChannel() {
  int channel = configGetValue("wifi.channel", "0").toInt();
  if (channel < 1 || channel > 13) return 0;
  return channel;
}

String wifiPhyConfig() {
  String phy = configGetValue("wifi.phy", "11g");
  phy.toLowerCase();
  if (phy == "b") phy = "11b";
  if (phy == "g") phy = "11g";
  if (phy == "n") phy = "11n";
  if (phy != "11b" && phy != "11g" && phy != "11n") phy = "11n";
  return phy;
}

WiFiPhyMode_t wifiPhyModeFromConfig() {
  String phy = wifiPhyConfig();
  if (phy == "11b") return WIFI_PHY_MODE_11B;
  if (phy == "11g") return WIFI_PHY_MODE_11G;
  return WIFI_PHY_MODE_11N;
}

const char* wifiPhyModeText(WiFiPhyMode_t mode) {
  if (mode == WIFI_PHY_MODE_11B) return "11b";
  if (mode == WIFI_PHY_MODE_11G) return "11g";
  return "11n";
}

void applyWifiPhyMode() {
  WiFi.setPhyMode(wifiPhyModeFromConfig());
}

float wifiOutputPowerDbm() {
  float power = configGetValue("wifi.power.dbm", "17.5").toFloat();
  if (power < 0.0f) power = 0.0f;
  if (power > 20.5f) power = 20.5f;
  return power;
}

void applyWifiOutputPower() {
  WiFi.setOutputPower(wifiOutputPowerDbm());
}

unsigned long wifiRetryDelayMs() {
  unsigned long delayMs = WIFI_RETRY_MIN_MS;
  for (uint8_t i = 1; i < wifiFailCount && delayMs < WIFI_RETRY_MAX_MS; i++) {
    delayMs *= 2;
  }
  if (delayMs > WIFI_RETRY_MAX_MS) delayMs = WIFI_RETRY_MAX_MS;
  return delayMs;
}

long wifiNextRetryRemainingMs() {
  if (WiFi.status() == WL_CONNECTED || wifiConnecting) return 0;
  long remaining = (long)(nextWifiWatchMs - millis());
  return remaining > 0 ? remaining : 0;
}

void ignoreWifiDisconnectEvents(unsigned long ms) {
  wifiIgnoreDisconnectEventsUntilMs = millis() + ms;
}

void disconnectWifiStation(bool eraseCredentials) {
  ignoreWifiDisconnectEvents(1500);
  WiFi.disconnect(false, eraseCredentials);
}

void resetWifiRadio() {
  eventLog("wifi radio reset");
  ignoreWifiDisconnectEvents(2500);
  WiFi.disconnect(false, false);
  delay(80);
  WiFi.mode(WIFI_OFF);
  fallbackApRunning = false;
  delay(300);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.mode(WIFI_STA);
  delay(120);
  applyWifiPhyMode();
  applyWifiOutputPower();
}

const char* wifiDisconnectReasonText(int reason) {
  switch (reason) {
    case WIFI_DISCONNECT_REASON_UNSPECIFIED: return "unspecified";
    case WIFI_DISCONNECT_REASON_AUTH_EXPIRE: return "auth_expire";
    case WIFI_DISCONNECT_REASON_AUTH_LEAVE: return "auth_leave";
    case WIFI_DISCONNECT_REASON_ASSOC_EXPIRE: return "assoc_expire";
    case WIFI_DISCONNECT_REASON_ASSOC_TOOMANY: return "assoc_toomany";
    case WIFI_DISCONNECT_REASON_NOT_AUTHED: return "not_authed";
    case WIFI_DISCONNECT_REASON_NOT_ASSOCED: return "not_assoced";
    case WIFI_DISCONNECT_REASON_ASSOC_LEAVE: return "assoc_leave";
    case WIFI_DISCONNECT_REASON_ASSOC_NOT_AUTHED: return "assoc_not_authed";
    case WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4way_handshake_timeout";
    case WIFI_DISCONNECT_REASON_BEACON_TIMEOUT: return "beacon_timeout";
    case WIFI_DISCONNECT_REASON_NO_AP_FOUND: return "no_ap_found";
    case WIFI_DISCONNECT_REASON_AUTH_FAIL: return "auth_fail";
    case WIFI_DISCONNECT_REASON_ASSOC_FAIL: return "assoc_fail";
    case WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT: return "handshake_timeout";
    default: return "other";
  }
}

String wifiStatusText() {
  String out = "status: " + String(WiFi.status()) + "\n";
  out += "connecting: " + String(wifiConnecting ? "yes" : "no") + "\n";
  out += "ssid: " + WiFi.SSID() + "\n";
  String dhcp = configGetValue("wifi.dhcp", "on");
  out += "mode: " + String(dhcp == "off" ? "static" : "dhcp") + "\n";
  out += "dhcp: " + dhcp + "\n";
  out += "timeout_ms: " + String(wifiConnectTimeoutMs()) + "\n";
  out += "channel: " + String(wifiConnectChannel()) + "\n";
  out += "phy: " + String(wifiPhyModeText(WiFi.getPhyMode())) + "\n";
  out += "tx_power_dbm: " + String(wifiOutputPowerDbm(), 1) + "\n";
  out += "rssi: " + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0) + "\n";
  out += "last_disconnect: " + String(lastWifiDisconnectReason) + " " + wifiDisconnectReasonText(lastWifiDisconnectReason) + "\n";
  out += "fail_count: " + String(wifiFailCount) + "\n";
  out += "attempt_events: " + String(wifiAttemptDisconnectEvents) + "\n";
  out += "next_retry_ms: " + String(wifiNextRetryRemainingMs()) + "\n";
  out += "ip: " + WiFi.localIP().toString() + "\n";
  out += "mask: " + WiFi.subnetMask().toString() + "\n";
  out += "gateway: " + WiFi.gatewayIP().toString() + "\n";
  out += "dns1: " + WiFi.dnsIP(0).toString() + "\n";
  out += "dns2: " + WiFi.dnsIP(1).toString() + "\n";
  out += "ap: " + String(fallbackApRunning ? WiFi.softAPIP().toString() : "off") + "\n";
  return out;
}

String wifiNetText() {
  String out = "wifi.dhcp=" + configGetValue("wifi.dhcp", "on") + "\n";
  out += "wifi.ip=" + configGetValue("wifi.ip", "") + "\n";
  out += "wifi.gateway=" + configGetValue("wifi.gateway", "") + "\n";
  out += "wifi.mask=" + configGetValue("wifi.mask", "") + "\n";
  out += "wifi.dns1=" + configGetValue("wifi.dns1", "") + "\n";
  out += "wifi.dns2=" + configGetValue("wifi.dns2", "") + "\n";
  out += "wifi.timeout.ms=" + configGetValue("wifi.timeout.ms", String(DEFAULT_WIFI_CONNECT_TIMEOUT_MS)) + "\n";
  out += "wifi.channel=" + String(wifiConnectChannel()) + "\n";
  out += "wifi.phy=" + wifiPhyConfig() + "\n";
  out += "wifi.power.dbm=" + String(wifiOutputPowerDbm(), 1) + "\n";
  out += "--- runtime ---\n" + wifiStatusText();
  return out;
}

bool applyWifiIpConfig() {
  IPAddress zero(0, 0, 0, 0);
  if (configGetValue("wifi.dhcp", "on") != "off") {
    WiFi.config(zero, zero, zero);
    return true;
  }

  IPAddress ip, gateway, mask, dns1, dns2;
  if (!parseIpAddress(configGetValue("wifi.ip", ""), ip) ||
      !parseIpAddress(configGetValue("wifi.gateway", ""), gateway) ||
      !parseIpAddress(configGetValue("wifi.mask", ""), mask)) {
    Serial.println(F("wifi static: invalid saved IP config, using DHCP"));
    eventLog("wifi static invalid");
    WiFi.config(zero, zero, zero);
    return false;
  }
  if (!parseIpAddress(configGetValue("wifi.dns1", ""), dns1)) dns1 = gateway;
  if (!parseIpAddress(configGetValue("wifi.dns2", ""), dns2)) dns2 = dns1;
  bool ok = WiFi.config(ip, gateway, mask, dns1, dns2);
  if (!ok) {
    Serial.println(F("wifi static: WiFi.config failed"));
    eventLog("wifi static failed");
  }
  return ok;
}

String wifiProfilePath(const String& name) {
  String safe = safeName(name);
  return safe.length() ? String(WIFI_PROFILE_DIR) + "/" + safe + ".txt" : "";
}

String wifiProfileText() {
  String out;
  out += "ssid=" + configGetValue("wifi.ssid", "") + "\n";
  out += "password=" + configGetValue("wifi.password", "") + "\n";
  out += "channel=" + String(wifiConnectChannel()) + "\n";
  out += "phy=" + wifiPhyConfig() + "\n";
  out += "power.dbm=" + String(wifiOutputPowerDbm(), 1) + "\n";
  out += "dhcp=" + configGetValue("wifi.dhcp", "on") + "\n";
  out += "ip=" + configGetValue("wifi.ip", "") + "\n";
  out += "gateway=" + configGetValue("wifi.gateway", "") + "\n";
  out += "mask=" + configGetValue("wifi.mask", "") + "\n";
  out += "dns1=" + configGetValue("wifi.dns1", "") + "\n";
  out += "dns2=" + configGetValue("wifi.dns2", "") + "\n";
  return out;
}

bool applyWifiProfileText(const String& text) {
  String ssid = keyValueFromText(text, "ssid", "");
  if (!ssid.length()) return false;
  configSetValue("wifi.ssid", ssid);
  configSetValue("wifi.password", keyValueFromText(text, "password", ""));
  configSetValue("wifi.channel", keyValueFromText(text, "channel", "0"));
  configSetValue("wifi.phy", keyValueFromText(text, "phy", "11g"));
  configSetValue("wifi.power.dbm", keyValueFromText(text, "power.dbm", "17.5"));
  configSetValue("wifi.dhcp", keyValueFromText(text, "dhcp", "on"));
  configSetValue("wifi.ip", keyValueFromText(text, "ip", ""));
  configSetValue("wifi.gateway", keyValueFromText(text, "gateway", ""));
  configSetValue("wifi.mask", keyValueFromText(text, "mask", ""));
  configSetValue("wifi.dns1", keyValueFromText(text, "dns1", ""));
  configSetValue("wifi.dns2", keyValueFromText(text, "dns2", ""));
  configSetValue("wifi.autoconnect", "on");
  applyWifiPhyMode();
  applyWifiOutputPower();
  return true;
}

void cmdWifiProfile(String args[], int argc) {
  if (!ensureFS()) return;
  LittleFS.mkdir(WIFI_PROFILE_DIR);
  String sub = argc >= 3 ? args[2] : "list";
  sub.toLowerCase();
  if (sub == "list") {
    Dir dir = LittleFS.openDir(WIFI_PROFILE_DIR);
    bool any = false;
    while (dir.next()) {
      if (dir.isDirectory()) continue;
      String path = dirChildPath(WIFI_PROFILE_DIR, dir.fileName());
      String name = basenameOf(path);
      if (name.endsWith(".txt")) name.remove(name.length() - 4);
      any = true;
      Serial.print(name);
      String text = readWholeFile(path);
      String ssid = keyValueFromText(text, "ssid", "");
      if (ssid.length()) {
        Serial.print(F("\t"));
        Serial.print(ssid);
        Serial.print(F("\tch "));
        Serial.print(keyValueFromText(text, "channel", "0"));
      }
      if (configGetValue("wifi.profile", "") == name) Serial.print(F("\tactive"));
      Serial.println();
    }
    if (!any) Serial.println(F("(empty)"));
  } else if (sub == "save") {
    if (argc < 4) { Serial.println(F("usage: wifi profile save <name>")); return; }
    String name = safeName(args[3]);
    String path = wifiProfilePath(name);
    if (!path.length()) { Serial.println(F("wifi profile: bad name")); return; }
    bool ok = writeWholeFile(path, wifiProfileText()) && configSetValue("wifi.profile", name);
    Serial.println(ok ? F("OK") : F("wifi profile: save failed"));
  } else if (sub == "use" || sub == "load") {
    if (argc < 4) { Serial.println(F("usage: wifi profile use <name> [reconnect]")); return; }
    String name = safeName(args[3]);
    String path = wifiProfilePath(name);
    String text = path.length() ? readWholeFile(path) : "";
    String previousSsid = configGetValue("wifi.ssid", "");
    if (!text.length() || !applyWifiProfileText(text)) { Serial.println(F("wifi profile: not found or invalid")); return; }
    configSetValue("wifi.profile", name);
    Serial.println(F("OK"));
    String ssid = configGetValue("wifi.ssid", "");
    bool forceReconnect = argc >= 5 && isReconnectArg(args[4]);
    if (forceReconnect || WiFi.status() != WL_CONNECTED || previousSsid != ssid || WiFi.SSID() != ssid) {
      beginWifiConnect(ssid, configGetValue("wifi.password", ""));
    } else {
      Serial.println(F("wifi profile: already connected"));
    }
  } else if (sub == "show") {
    if (argc < 4) { Serial.println(F("usage: wifi profile show <name>")); return; }
    String text = readWholeFile(wifiProfilePath(args[3]));
    if (!text.length()) { Serial.println(F("wifi profile: not found")); return; }
    Serial.print(text);
  } else if (sub == "rm" || sub == "remove") {
    if (argc < 4) { Serial.println(F("usage: wifi profile rm <name>")); return; }
    String name = safeName(args[3]);
    bool ok = LittleFS.remove(wifiProfilePath(name));
    if (configGetValue("wifi.profile", "") == name) configRemoveValue("wifi.profile");
    Serial.println(ok ? F("OK") : F("wifi profile: not found"));
  } else {
    Serial.println(F("usage: wifi profile list|save|use|show|rm"));
  }
}

void cmdDiagWifi() {
  Serial.println(F("== wifi diagnosis =="));
  Serial.print(wifiStatusText());
  if (WiFi.status() == WL_CONNECTED) {
    long rssi = WiFi.RSSI();
    if (rssi < -75) Serial.println(F("warn: weak signal"));
    else if (rssi > -35) Serial.println(F("note: very strong signal; lower tx power if packets are lost"));
  }
  if (lastWifiDisconnectReason) {
    Serial.print(F("last_reason_hint: "));
    switch (lastWifiDisconnectReason) {
      case WIFI_DISCONNECT_REASON_NO_AP_FOUND:
        Serial.println(F("AP not found; check SSID, channel, or location"));
        break;
      case WIFI_DISCONNECT_REASON_AUTH_FAIL:
      case WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT:
      case WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT:
      case WIFI_DISCONNECT_REASON_AUTH_EXPIRE:
        Serial.println(F("authentication/handshake; check password, WPA mode, PMF/802.11r, or SDK state"));
        break;
      case WIFI_DISCONNECT_REASON_BEACON_TIMEOUT:
        Serial.println(F("beacon timeout; check interference, AP load, sleep mode, or power"));
        break;
      default:
        Serial.println(F("see disconnect reason and dmesg"));
        break;
    }
  }
  Serial.print(F("profile: "));
  Serial.println(configGetValue("wifi.profile", "(none)"));
  Serial.println(F("repair_order: wifi recover -> wifi sdkreset --yes -> check AP WPA2-PSK/AES 2.4GHz"));
}

void cmdWifi(String args[], int argc) {
  if (argc < 2) {
    Serial.println(F("usage: wifi scan|status|diag|profile|connect|save|forget|dhcp|static|net|timeout|channel|phy|power|recover|sdkreset|autoconnect|reconnect|wait|disconnect|ip|mac|mode|ap|hostname"));
    return;
  }
  String sub = args[1];
  sub.toLowerCase();

  if (sub == "scan") {
    if (wifiConnecting) {
      disconnectWifiStation(false);
      wifiConnecting = false;
      wifiWasConnected = false;
      delay(100);
    }
    Serial.println(F("scanning..."));
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
      Serial.print(i); Serial.print(F(": "));
      Serial.print(WiFi.SSID(i));
      Serial.print(F("  "));
      Serial.print(WiFi.RSSI(i));
      Serial.print(F(" dBm  ch "));
      Serial.println(WiFi.channel(i));
    }
    if (n <= 0) Serial.println(F("no networks"));
  } else if (sub == "status") {
    Serial.print(wifiStatusText());
  } else if (sub == "net") {
    Serial.print(wifiNetText());
  } else if (sub == "diag") {
    cmdDiagWifi();
  } else if (sub == "profile") {
    cmdWifiProfile(args, argc);
  } else if (sub == "dhcp") {
    if (argc < 3) {
      Serial.println(configGetValue("wifi.dhcp", "on"));
      return;
    }
    String value = args[2]; value.toLowerCase();
    if (value != "on" && value != "off") { Serial.println(F("usage: wifi dhcp on|off [reconnect]")); return; }
    bool ok = configSetValue("wifi.dhcp", value);
    Serial.println(ok ? F("OK") : F("wifi: dhcp save failed"));
    if (ok && argc >= 4 && isReconnectArg(args[3])) {
      String ssid = configGetValue("wifi.ssid", "");
      String password = configGetValue("wifi.password", "");
      if (ssid.length()) beginWifiConnect(ssid, password);
    }
  } else if (sub == "static") {
    if (argc < 5) {
      Serial.println(F("usage: wifi static <ip> <gateway> <mask> [dns1] [dns2] [reconnect]"));
      return;
    }
    IPAddress ip, gateway, mask, dns1, dns2;
    if (!parseIpAddress(args[2], ip) || !parseIpAddress(args[3], gateway) || !parseIpAddress(args[4], mask)) {
      Serial.println(F("wifi static: bad ip/gateway/mask"));
      return;
    }
    bool reconnect = false;
    String dns1Text = args[3];
    String dns2Text = "";
    if (argc >= 6) {
      if (isReconnectArg(args[5])) reconnect = true;
      else dns1Text = args[5];
    }
    if (argc >= 7) {
      if (isReconnectArg(args[6])) reconnect = true;
      else dns2Text = args[6];
    }
    if (argc >= 8 && isReconnectArg(args[7])) reconnect = true;
    if (!parseIpAddress(dns1Text, dns1) || (dns2Text.length() && !parseIpAddress(dns2Text, dns2))) {
      Serial.println(F("wifi static: bad dns"));
      return;
    }
    bool ok = configSetValue("wifi.dhcp", "off") &&
              configSetValue("wifi.ip", args[2]) &&
              configSetValue("wifi.gateway", args[3]) &&
              configSetValue("wifi.mask", args[4]) &&
              configSetValue("wifi.dns1", dns1Text);
    if (ok) {
      if (dns2Text.length()) ok = configSetValue("wifi.dns2", dns2Text);
      else configRemoveValue("wifi.dns2");
    }
    Serial.println(ok ? F("OK") : F("wifi: static save failed"));
    if (ok && reconnect) {
      String ssid = configGetValue("wifi.ssid", "");
      String password = configGetValue("wifi.password", "");
      if (ssid.length()) beginWifiConnect(ssid, password);
    }
  } else if (sub == "connect") {
    if (argc < 4) { Serial.println(F("usage: wifi connect <ssid> <password>")); return; }
    beginWifiConnect(args[2], args[3]);
  } else if (sub == "save") {
    if (argc < 4) { Serial.println(F("usage: wifi save <ssid> <password>")); return; }
    bool ok = configSetValue("wifi.ssid", args[2]) &&
              configSetValue("wifi.password", args[3]) &&
              configSetValue("wifi.autoconnect", "on");
    Serial.println(ok ? F("OK") : F("wifi: save failed"));
  } else if (sub == "forget") {
    configRemoveValue("wifi.ssid");
    configRemoveValue("wifi.password");
    configSetValue("wifi.autoconnect", "off");
    disconnectWifiStation(true);
    wifiConnecting = false;
    wifiWasConnected = false;
    Serial.println(F("OK"));
  } else if (sub == "autoconnect") {
    if (argc < 3) {
      Serial.println(configGetValue("wifi.autoconnect", "off"));
      return;
    }
    String value = args[2]; value.toLowerCase();
    if (value != "on" && value != "off") { Serial.println(F("usage: wifi autoconnect on|off")); return; }
    Serial.println(configSetValue("wifi.autoconnect", value) ? F("OK") : F("wifi: autoconnect failed"));
  } else if (sub == "reconnect") {
    String ssid = configGetValue("wifi.ssid", "");
    String password = configGetValue("wifi.password", "");
    if (!ssid.length()) { Serial.println(F("wifi: no saved network")); return; }
    beginWifiConnect(ssid, password);
  } else if (sub == "recover") {
    resetWifiRadio();
    wifiConnecting = false;
    wifiWasConnected = false;
    wifiFailCount = 0;
    wifiAttemptDisconnectEvents = 0;
    nextWifiWatchMs = 0;
    connectSavedWifi();
  } else if (sub == "sdkreset") {
    if (argc < 3 || args[2] != "--yes") { Serial.println(F("usage: wifi sdkreset --yes")); return; }
    disconnectWifiStation(true);
    Serial.println(ESP.eraseConfig() ? F("wifi sdk config erased; rebooting") : F("wifi sdk config erase failed"));
    delay(250);
    ESP.restart();
  } else if (sub == "timeout") {
    if (argc < 3) {
      Serial.println(wifiConnectTimeoutMs());
      return;
    }
    unsigned long ms = (unsigned long)args[2].toInt();
    if (ms < 10000UL || ms > 120000UL) { Serial.println(F("usage: wifi timeout <10000-120000_ms>")); return; }
    Serial.println(configSetValue("wifi.timeout.ms", String(ms)) ? F("OK") : F("wifi: timeout save failed"));
  } else if (sub == "channel") {
    if (argc < 3) {
      Serial.println(wifiConnectChannel());
      return;
    }
    int channel = args[2].toInt();
    if (channel < 0 || channel > 13) { Serial.println(F("usage: wifi channel <0|1-13>")); return; }
    Serial.println(configSetValue("wifi.channel", String(channel)) ? F("OK") : F("wifi: channel save failed"));
  } else if (sub == "phy") {
    if (argc < 3) {
      Serial.println(wifiPhyConfig());
      return;
    }
    String phy = args[2];
    phy.toLowerCase();
    if (phy == "b") phy = "11b";
    if (phy == "g") phy = "11g";
    if (phy == "n") phy = "11n";
    if (phy != "11b" && phy != "11g" && phy != "11n") { Serial.println(F("usage: wifi phy 11b|11g|11n")); return; }
    bool ok = configSetValue("wifi.phy", phy);
    if (ok) applyWifiPhyMode();
    Serial.println(ok ? F("OK") : F("wifi: phy save failed"));
  } else if (sub == "power") {
    if (argc < 3) {
      Serial.println(String(wifiOutputPowerDbm(), 1));
      return;
    }
    float power = args[2].toFloat();
    if (power < 0.0f || power > 20.5f) { Serial.println(F("usage: wifi power <0.0-20.5_dBm>")); return; }
    bool ok = configSetValue("wifi.power.dbm", String(power, 1));
    if (ok) applyWifiOutputPower();
    Serial.println(ok ? F("OK") : F("wifi: power save failed"));
  } else if (sub == "wait") {
    unsigned long timeoutMs = (argc >= 3 ? (unsigned long)args[2].toInt() : 15UL) * 1000UL;
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
      processWifi();
      delay(50);
      yield();
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? F("connected") : F("not connected"));
    if (WiFi.status() == WL_CONNECTED) Serial.println(WiFi.localIP());
  } else if (sub == "disconnect") {
    disconnectWifiStation(false);
    wifiConnecting = false;
    wifiWasConnected = false;
    Serial.println(F("disconnected"));
  } else if (sub == "ip") {
    Serial.println(WiFi.localIP());
  } else if (sub == "mac") {
    Serial.println(WiFi.macAddress());
  } else if (sub == "mode") {
    Serial.println(F("station"));
    WiFi.mode(WIFI_STA);
  } else if (sub == "ap") {
    cmdAp(args + 1, argc - 1);
  } else if (sub == "hostname") {
    if (argc < 3) Serial.println(configGetValue("wifi.hostname", "kernelesp"));
    else {
      configSetValue("wifi.hostname", args[2]);
      WiFi.hostname(args[2].c_str());
      Serial.println(F("OK"));
    }
  } else {
    Serial.println(F("wifi: unknown subcommand"));
  }
}

void cmdDmesg() {
  for (uint8_t i = 0; i < DMESG_LINES; i++) {
    uint8_t idx = (dmesgHead + i) % DMESG_LINES;
    if (dmesgRing[idx].text[0] == '\0') continue;
    Serial.print('[');
    Serial.print(dmesgRing[idx].ms / 1000);
    Serial.print(F("] "));
    Serial.println(dmesgRing[idx].text);
  }
}

void readBmeCalibration(uint8_t addr) {
  bmeCal.t1 = i2cRead16LE(addr, 0x88);
  bmeCal.t2 = i2cReadS16LE(addr, 0x8A);
  bmeCal.t3 = i2cReadS16LE(addr, 0x8C);
  bmeCal.p1 = i2cRead16LE(addr, 0x8E);
  bmeCal.p2 = i2cReadS16LE(addr, 0x90);
  bmeCal.p3 = i2cReadS16LE(addr, 0x92);
  bmeCal.p4 = i2cReadS16LE(addr, 0x94);
  bmeCal.p5 = i2cReadS16LE(addr, 0x96);
  bmeCal.p6 = i2cReadS16LE(addr, 0x98);
  bmeCal.p7 = i2cReadS16LE(addr, 0x9A);
  bmeCal.p8 = i2cReadS16LE(addr, 0x9C);
  bmeCal.p9 = i2cReadS16LE(addr, 0x9E);
  bmeCal.h1 = i2cRead8(addr, 0xA1);
  bmeCal.h2 = i2cReadS16LE(addr, 0xE1);
  bmeCal.h3 = i2cRead8(addr, 0xE3);
  uint8_t e4 = i2cRead8(addr, 0xE4);
  uint8_t e5 = i2cRead8(addr, 0xE5);
  uint8_t e6 = i2cRead8(addr, 0xE6);
  bmeCal.h4 = (int16_t)((e4 << 4) | (e5 & 0x0F));
  if (bmeCal.h4 & 0x0800) bmeCal.h4 |= 0xF000;
  bmeCal.h5 = (int16_t)((e6 << 4) | (e5 >> 4));
  if (bmeCal.h5 & 0x0800) bmeCal.h5 |= 0xF000;
  bmeCal.h6 = (int8_t)i2cRead8(addr, 0xE7);
}

bool sensorBegin(uint8_t addr, uint8_t sda, uint8_t scl) {
  Wire.begin(sda, scl);
  Wire.setClock(100000);
  uint8_t id = i2cRead8(addr, 0xD0);
  if (id != 0x60 && id != 0x58) {
    sensorReady = false;
    return false;
  }
  sensorAddress = addr;
  sensorHasHumidity = id == 0x60;
  readBmeCalibration(addr);
  if (sensorHasHumidity) i2cWrite8(addr, 0xF2, 0x01);
  i2cWrite8(addr, 0xF4, 0x27);
  i2cWrite8(addr, 0xF5, 0xA0);
  sensorReady = true;
  eventLog(String(sensorHasHumidity ? "BME280" : "BMP280") + " sensor ready");
  return true;
}

bool sensorAutoBegin() {
  uint8_t sda = configGetValue("sensor.sda", "4").toInt();
  uint8_t scl = configGetValue("sensor.scl", "5").toInt();
  String addrText = configGetValue("sensor.addr", "");
  if (addrText.length()) return sensorBegin(parseHexOrDec(addrText), sda, scl);
  if (sensorBegin(0x76, sda, scl)) return true;
  return sensorBegin(0x77, sda, scl);
}

SensorReading readBme() {
  SensorReading r = SensorReading();
  if (!sensorReady && !sensorAutoBegin()) return r;

  uint8_t data[8];
  Wire.beginTransmission(sensorAddress);
  Wire.write(0xF7);
  if (Wire.endTransmission() != 0) { sensorReady = false; return r; }
  Wire.requestFrom((int)sensorAddress, sensorHasHumidity ? 8 : 6);
  uint8_t need = sensorHasHumidity ? 8 : 6;
  for (uint8_t i = 0; i < need; i++) {
    if (!Wire.available()) { sensorReady = false; return r; }
    data[i] = Wire.read();
  }

  int32_t adcP = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | (data[2] >> 4);
  int32_t adcT = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | (data[5] >> 4);
  int32_t adcH = sensorHasHumidity ? (((uint32_t)data[6] << 8) | data[7]) : 0;

  int32_t var1 = ((((adcT >> 3) - ((int32_t)bmeCal.t1 << 1))) * ((int32_t)bmeCal.t2)) >> 11;
  int32_t var2 = (((((adcT >> 4) - ((int32_t)bmeCal.t1)) * ((adcT >> 4) - ((int32_t)bmeCal.t1))) >> 12) * ((int32_t)bmeCal.t3)) >> 14;
  bmeTFine = var1 + var2;
  float temp = ((bmeTFine * 5 + 128) >> 8) / 100.0f;

  int64_t pvar1 = ((int64_t)bmeTFine) - 128000;
  int64_t pvar2 = pvar1 * pvar1 * (int64_t)bmeCal.p6;
  pvar2 += (pvar1 * (int64_t)bmeCal.p5) << 17;
  pvar2 += ((int64_t)bmeCal.p4) << 35;
  pvar1 = ((pvar1 * pvar1 * (int64_t)bmeCal.p3) >> 8) + ((pvar1 * (int64_t)bmeCal.p2) << 12);
  pvar1 = (((((int64_t)1) << 47) + pvar1)) * ((int64_t)bmeCal.p1) >> 33;
  if (pvar1 == 0) return r;
  int64_t p = 1048576 - adcP;
  p = (((p << 31) - pvar2) * 3125) / pvar1;
  pvar1 = (((int64_t)bmeCal.p9) * (p >> 13) * (p >> 13)) >> 25;
  pvar2 = (((int64_t)bmeCal.p8) * p) >> 19;
  p = ((p + pvar1 + pvar2) >> 8) + (((int64_t)bmeCal.p7) << 4);

  float hum = NAN;
  if (sensorHasHumidity) {
    int32_t v = bmeTFine - 76800;
    v = (((((adcH << 14) - (((int32_t)bmeCal.h4) << 20) - (((int32_t)bmeCal.h5) * v)) + 16384) >> 15) *
         (((((((v * ((int32_t)bmeCal.h6)) >> 10) * (((v * ((int32_t)bmeCal.h3)) >> 11) + 32768)) >> 10) + 2097152) *
          ((int32_t)bmeCal.h2) + 8192) >> 14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)bmeCal.h1)) >> 4);
    v = constrain(v, 0, 419430400);
    hum = (v >> 12) / 1024.0f;
  }

  r.ok = true;
  r.hasHumidity = sensorHasHumidity;
  r.temperatureC = temp;
  r.pressureHpa = (p / 256.0f) / 100.0f;
  r.humidityPct = hum;
  return r;
}

void cmdI2c(String args[], int argc) {
  if (argc < 2 || args[1] == "scan") {
    uint8_t sda = argc >= 3 ? pinFromToken(args[2]) : configGetValue("sensor.sda", "4").toInt();
    uint8_t scl = argc >= 4 ? pinFromToken(args[3]) : configGetValue("sensor.scl", "5").toInt();
    Wire.begin(sda, scl);
    Serial.print(F("i2c scan sda GPIO"));
    Serial.print(sda);
    Serial.print(F(" scl GPIO"));
    Serial.println(scl);
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++) {
      if (i2cPresent(addr)) {
        found = true;
        Serial.print(F("0x"));
        if (addr < 16) Serial.print('0');
        Serial.println(addr, HEX);
      }
      yield();
    }
    if (!found) Serial.println(F("(none)"));
  } else {
    Serial.println(F("usage: i2c scan [sda] [scl]"));
  }
}

void cmdPcf(String args[], int argc) {
  if (argc < 3) { Serial.println(F("usage: pcf read|write <addr> [value]")); return; }
  String sub = args[1]; sub.toLowerCase();
  uint8_t addr = parseHexOrDec(args[2]);
  Wire.begin(configGetValue("sensor.sda", "4").toInt(), configGetValue("sensor.scl", "5").toInt());
  if (sub == "read") {
    Wire.requestFrom((int)addr, 1);
    if (!Wire.available()) { Serial.println(F("pcf: no response")); return; }
    Serial.println(Wire.read(), HEX);
  } else if (sub == "write") {
    if (argc < 4) { Serial.println(F("usage: pcf write <addr> <0-255>")); return; }
    Wire.beginTransmission(addr);
    Wire.write((uint8_t)args[3].toInt());
    Serial.println(Wire.endTransmission() == 0 ? F("OK") : F("pcf: write failed"));
  } else {
    Serial.println(F("usage: pcf read|write <addr> [value]"));
  }
}

bool mcpWriteReg(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

int mcpReadReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return -1;
  Wire.requestFrom((int)addr, 1);
  return Wire.available() ? Wire.read() : -1;
}

void cmdMcp(String args[], int argc) {
  if (argc < 3) { Serial.println(F("usage: mcp init|read|write <addr> ...")); return; }
  String sub = args[1]; sub.toLowerCase();
  uint8_t addr = parseHexOrDec(args[2]);
  Wire.begin(configGetValue("sensor.sda", "4").toInt(), configGetValue("sensor.scl", "5").toInt());
  if (sub == "init") {
    if (argc < 5) { Serial.println(F("usage: mcp init <addr> <iodira> <iodirb>")); return; }
    bool ok = mcpWriteReg(addr, 0x00, parseHexOrDec(args[3])) && mcpWriteReg(addr, 0x01, parseHexOrDec(args[4]));
    Serial.println(ok ? F("OK") : F("mcp: init failed"));
  } else if (sub == "read") {
    if (argc < 4) { Serial.println(F("usage: mcp read <addr> a|b")); return; }
    uint8_t reg = args[3] == "b" ? 0x13 : 0x12;
    int value = mcpReadReg(addr, reg);
    if (value < 0) Serial.println(F("mcp: read failed"));
    else Serial.println(value, HEX);
  } else if (sub == "write") {
    if (argc < 5) { Serial.println(F("usage: mcp write <addr> a|b <0-255>")); return; }
    uint8_t reg = args[3] == "b" ? 0x15 : 0x14;
    Serial.println(mcpWriteReg(addr, reg, parseHexOrDec(args[4])) ? F("OK") : F("mcp: write failed"));
  } else {
    Serial.println(F("usage: mcp init|read|write <addr> ..."));
  }
}

void cmdSensor(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "read";
  sub.toLowerCase();
  if (sub == "begin") {
    uint8_t addr = argc >= 3 ? parseHexOrDec(args[2]) : parseHexOrDec(configGetValue("sensor.addr", "0x76"));
    uint8_t sda = argc >= 4 ? pinFromToken(args[3]) : configGetValue("sensor.sda", "4").toInt();
    uint8_t scl = argc >= 5 ? pinFromToken(args[4]) : configGetValue("sensor.scl", "5").toInt();
    Serial.println(sensorBegin(addr, sda, scl) ? F("OK") : F("sensor: not found"));
  } else if (sub == "scan") {
    String fakeArgs[] = {"i2c", "scan"};
    cmdI2c(fakeArgs, 2);
  } else if (sub == "save") {
    if (argc >= 3) configSetValue("sensor.addr", args[2]);
    if (argc >= 4) configSetValue("sensor.sda", String(pinFromToken(args[3])));
    if (argc >= 5) configSetValue("sensor.scl", String(pinFromToken(args[4])));
    configSetValue("sensor.autostart", "on");
    Serial.println(F("OK"));
  } else if (sub == "autostart") {
    if (argc < 3) { Serial.println(configGetValue("sensor.autostart", "off")); return; }
    String value = args[2]; value.toLowerCase();
    if (value != "on" && value != "off") { Serial.println(F("usage: sensor autostart on|off")); return; }
    Serial.println(configSetValue("sensor.autostart", value) ? F("OK") : F("sensor: config failed"));
  } else if (sub == "status") {
    Serial.print(F("ready: "));
    Serial.println(sensorReady ? F("yes") : F("no"));
    Serial.print(F("addr: 0x"));
    if (sensorAddress < 16) Serial.print('0');
    Serial.println(sensorAddress, HEX);
    Serial.print(F("type: "));
    Serial.println(sensorHasHumidity ? F("BME280") : F("BMP280/unknown"));
    Serial.print(F("autostart: "));
    Serial.println(configGetValue("sensor.autostart", "off"));
  } else if (sub == "read") {
    Serial.print(sensorText());
  } else {
    Serial.println(F("usage: sensor begin|scan|read|status|save|autostart"));
  }
}

int findRelay(const String& name) {
  for (uint8_t i = 0; i < MAX_RELAYS; i++) {
    if (relays[i].configured && relays[i].name == name) return i;
  }
  return -1;
}

bool applyRelay(uint8_t idx, bool on) {
  if (idx >= MAX_RELAYS || !relays[idx].configured) return false;
  bool changed = relays[idx].state != on;
  if (configGetValue("dryrun", "off") != "on") {
    pinMode(relays[idx].pin, OUTPUT);
    digitalWrite(relays[idx].pin, relays[idx].activeLow ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
  } else {
    Serial.print(F("[dryrun] relay "));
    Serial.print(relays[idx].name);
    Serial.println(on ? F(" on") : F(" off"));
  }
  relays[idx].state = on;
  if (!on) relayPulseUntil[idx] = 0;
  if (changed) eventLog("relay " + relays[idx].name + (on ? " on" : " off"));
  return changed;
}

void pulseRelay(uint8_t idx, unsigned long ms) {
  if (idx >= MAX_RELAYS || !relays[idx].configured) return;
  if (ms < 50) ms = 50;
  if (ms > 600000UL) ms = 600000UL;
  bool changed = applyRelay(idx, true);
  relayPulseUntil[idx] = millis() + ms;
  if (changed) saveRelays();
}

void processRelayPulses() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_RELAYS; i++) {
    if (!relayPulseUntil[i]) continue;
    if ((long)(now - relayPulseUntil[i]) >= 0) {
      relayPulseUntil[i] = 0;
      if (applyRelay(i, false)) saveRelays();
    }
  }
}

void saveRelays() {
  if (!ensureFS()) return;
  String out;
  for (uint8_t i = 0; i < MAX_RELAYS; i++) {
    if (!relays[i].configured) continue;
    out += relays[i].name + ",";
    out += String(relays[i].pin) + ",";
    out += (relays[i].activeLow ? "low" : "high");
    out += ",";
    out += relays[i].bootState;
    out += ",";
    out += (relays[i].state ? "1" : "0");
    out += "\n";
  }
  writeWholeFile(CONF_RELAYS, out);
}

void loadRelays() {
  for (uint8_t i = 0; i < MAX_RELAYS; i++) {
    relays[i] = Relay();
    relayPulseUntil[i] = 0;
  }
  if (!fsReady || !LittleFS.exists(CONF_RELAYS)) return;
  File file = LittleFS.open(CONF_RELAYS, "r");
  if (!file) return;
  uint8_t idx = 0;
  while (file.available() && idx < MAX_RELAYS) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    int p4 = line.indexOf(',', p3 + 1);
    if (p1 <= 0 || p2 <= p1 || p3 <= p2) continue;
    relays[idx].name = line.substring(0, p1);
    relays[idx].pin = line.substring(p1 + 1, p2).toInt();
    relays[idx].activeLow = line.substring(p2 + 1, p3) != "high";
    relays[idx].bootState = p4 > p3 ? line.substring(p3 + 1, p4) : line.substring(p3 + 1);
    relays[idx].bootState.trim();
    relays[idx].state = p4 > p3 ? line.substring(p4 + 1).toInt() != 0 : false;
    relays[idx].configured = true;
    if (relays[idx].bootState == "on") applyRelay(idx, true);
    else if (relays[idx].bootState == "last") applyRelay(idx, relays[idx].state);
    else applyRelay(idx, false);
    idx++;
  }
  file.close();
}

void cmdRelay(String args[], int argc) {
  if (argc < 2) {
    Serial.println(F("usage: relay add|rm|on|off|toggle|pulse|status|boot|save|load"));
    return;
  }
  String sub = args[1];
  sub.toLowerCase();
  if (sub == "add") {
    if (argc < 5) { Serial.println(F("usage: relay add <name> <pin> active_low|active_high")); return; }
    if (findRelay(args[2]) >= 0) { Serial.println(F("relay: already exists")); return; }
    int pin;
    if (!resolvePin(args[3], pin)) return;
    String mode = args[4]; mode.toLowerCase();
    if (mode != "active_low" && mode != "active_high") { Serial.println(F("relay: mode must be active_low or active_high")); return; }
    for (uint8_t i = 0; i < MAX_RELAYS; i++) {
      if (!relays[i].configured) {
        relays[i].name = args[2];
        relays[i].pin = pin;
        relays[i].activeLow = mode == "active_low";
        relays[i].bootState = "off";
        relays[i].configured = true;
        applyRelay(i, false);
        saveRelays();
        Serial.println(F("OK"));
        return;
      }
    }
    Serial.println(F("relay: table full"));
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: relay rm <name>")); return; }
    int idx = findRelay(args[2]);
    if (idx < 0) { Serial.println(F("relay: not found")); return; }
    applyRelay(idx, false);
    relays[idx] = Relay();
    relayPulseUntil[idx] = 0;
    saveRelays();
    Serial.println(F("OK"));
  } else if (sub == "on" || sub == "off" || sub == "toggle" || sub == "pulse") {
    if (argc < 3) { Serial.println(F("usage: relay on|off|toggle|pulse <name> [ms]")); return; }
    int idx = findRelay(args[2]);
    if (idx < 0) { Serial.println(F("relay: not found")); return; }
    bool changed = false;
    if (sub == "pulse") {
      pulseRelay(idx, argc >= 4 ? args[3].toInt() : 1000);
    } else if (sub == "toggle") {
      changed = applyRelay(idx, !relays[idx].state);
    } else {
      changed = applyRelay(idx, sub == "on");
    }
    if (changed) saveRelays();
    Serial.println(F("OK"));
  } else if (sub == "status") {
    bool any = false;
    for (uint8_t i = 0; i < MAX_RELAYS; i++) {
      if (!relays[i].configured) continue;
      any = true;
      Serial.print(relays[i].name);
      Serial.print(F(" GPIO"));
      Serial.print(relays[i].pin);
      Serial.print(relays[i].activeLow ? F(" active_low ") : F(" active_high "));
      Serial.print(relays[i].state ? F("ON boot=") : F("OFF boot="));
      Serial.println(relays[i].bootState);
    }
    if (!any) Serial.println(F("(no relays)"));
  } else if (sub == "boot") {
    if (argc < 4) { Serial.println(F("usage: relay boot <name> off|on|last")); return; }
    int idx = findRelay(args[2]);
    if (idx < 0) { Serial.println(F("relay: not found")); return; }
    String boot = args[3]; boot.toLowerCase();
    if (boot != "off" && boot != "on" && boot != "last") { Serial.println(F("relay: boot must be off, on, or last")); return; }
    relays[idx].bootState = boot;
    saveRelays();
    Serial.println(F("OK"));
  } else if (sub == "save") {
    saveRelays();
    Serial.println(F("OK"));
  } else if (sub == "load") {
    loadRelays();
    Serial.println(F("OK"));
  } else {
    Serial.println(F("relay: unknown subcommand"));
  }
}

void runScriptText(String content) {
  content.replace("\r", "\n");
  int start = 0;
  int contentLen = content.length();
  String pending;
  int pendingBraces = 0;
  bool pendingMultilineIf = false;
  while (start < contentLen) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = contentLen;
    String line = content.substring(start, end);
    line = stripCLineComment(line);
    line.trim();
    start = end + 1;
    if (!line.length() || line.startsWith("#") || line.startsWith("//")) continue;
    String lineLower = line;
    lineLower.toLowerCase();
    bool startsPending = !pending.length();
    if (startsPending && lineLower.startsWith("if ") && braceDepthDelta(line) > 0) pendingMultilineIf = true;
    if (pending.length()) pending += " ";
    pending += line;
    pendingBraces += braceDepthDelta(line);
    if (pendingBraces > 0) continue;
    if (pendingBraces < 0) pendingBraces = 0;
    Serial.print(F("[sh] "));
    Serial.println(pending);
    if (pendingMultilineIf) Serial.println(F("sh: multiline if blocks not supported; use one-line if block"));
    else executeLine(pending);
    pending = "";
    pendingMultilineIf = false;
    yield();
  }
  pending.trim();
  if (pending.length()) {
    Serial.print(F("[sh] "));
    Serial.println(pending);
    if (pendingMultilineIf) Serial.println(F("sh: multiline if blocks not supported; use one-line if block"));
    else executeLine(pending);
  }
}

void runScriptFile(const String& path) {
  if (!ensureFS()) return;
  String normalized = normalizePath(path);
  File file = LittleFS.open(normalized, "r");
  if (!file || file.isDirectory()) { Serial.println(F("sh: cannot open script")); return; }
  String content = file.readString();
  file.close();
  runScriptText(content);
}

bool validateScriptFile(const String& path) {
  if (!ensureFS()) return false;
  String normalized = normalizePath(path);
  File file = LittleFS.open(normalized, "r");
  if (!file || file.isDirectory()) { Serial.println(F("sh: cannot open script")); return false; }
  bool ok = true;
  uint16_t lineNo = 0;
  String pending;
  int pendingBraces = 0;
  bool pendingMultilineIf = false;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line = stripCLineComment(line);
    line.trim();
    lineNo++;
    if (!line.length() || line.startsWith("#") || line.startsWith("//")) continue;
    String lineLower = line;
    lineLower.toLowerCase();
    bool startsPending = !pending.length();
    if (startsPending && lineLower.startsWith("if ") && braceDepthDelta(line) > 0) pendingMultilineIf = true;
    if (pending.length()) pending += " ";
    pending += line;
    pendingBraces += braceDepthDelta(line);
    if (pendingBraces > 0) continue;
    if (pendingBraces < 0) pendingBraces = 0;
    String args[MAX_ARGS];
    int argc = splitArgs(pending, args, MAX_ARGS);
    if (argc == 0) continue;
    String cmd = args[0];
    cmd.toLowerCase();
    if (pendingMultilineIf) {
      ok = false;
      Serial.print(F("line "));
      Serial.print(lineNo);
      Serial.println(F(": multiline if blocks are not supported"));
    } else if (!isKnownCommand(cmd) && !functionExists(cmd)) {
      ok = false;
      Serial.print(F("line "));
      Serial.print(lineNo);
      Serial.print(F(": unknown command "));
      Serial.println(cmd);
    }
    pending = "";
    pendingMultilineIf = false;
    yield();
  }
  if (pending.length()) {
    ok = false;
    Serial.println(F("sh: unclosed block"));
  }
  file.close();
  if (ok) Serial.println(F("OK"));
  return ok;
}

void saveTimers() {
  if (!ensureFS()) return;
  String out;
  for (uint8_t i = 0; i < MAX_TIMERS; i++) {
    if (!timers[i].active) continue;
    out += String(timers[i].id) + ",";
    out += String(timers[i].intervalMs) + ",";
    out += (timers[i].repeat ? "every" : "once");
    out += ",";
    out += timers[i].command + "\n";
  }
  writeWholeFile(CONF_TIMERS, out);
}

void loadTimers() {
  for (uint8_t i = 0; i < MAX_TIMERS; i++) timers[i] = TimerJob();
  if (!fsReady || !LittleFS.exists(CONF_TIMERS)) return;
  File file = LittleFS.open(CONF_TIMERS, "r");
  if (!file) return;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    if (p1 <= 0 || p2 <= p1) continue;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
      if (!timers[i].active) {
        timers[i].id = line.substring(0, p1).toInt();
        timers[i].intervalMs = line.substring(p1 + 1, p2).toInt();
        if (p3 > p2) {
          String mode = line.substring(p2 + 1, p3);
          timers[i].repeat = mode != "once";
          timers[i].command = line.substring(p3 + 1);
        } else {
          timers[i].repeat = true;
          timers[i].command = line.substring(p2 + 1);
        }
        timers[i].nextRun = millis() + timers[i].intervalMs;
        timers[i].active = timers[i].intervalMs > 0 && timers[i].command.length() > 0;
        if (timers[i].id >= nextTimerId) nextTimerId = timers[i].id + 1;
        break;
      }
    }
  }
  file.close();
}

void cmdTimer(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: timer add|every|once|after|list|rm|clear")); return; }
  String sub = args[1]; sub.toLowerCase();
  if (sub == "add" || sub == "every" || sub == "once" || sub == "after") {
    if (argc < 4) { Serial.println(F("usage: timer every|once <ms> <command>")); return; }
    unsigned long ms = args[2].toInt();
    if (ms < 100) { Serial.println(F("timer: interval must be >=100ms")); return; }
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
      if (!timers[i].active) {
        timers[i].id = nextTimerId++;
        timers[i].intervalMs = ms;
        timers[i].nextRun = millis() + ms;
        timers[i].command = joinArgs(args, argc, 3);
        timers[i].repeat = sub == "add" || sub == "every";
        timers[i].active = true;
        saveTimers();
        Serial.print(F("timer id "));
        Serial.println(timers[i].id);
        return;
      }
    }
    Serial.println(F("timer: table full"));
  } else if (sub == "list") {
    bool any = false;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
      if (!timers[i].active) continue;
      any = true;
      Serial.print(timers[i].id);
      Serial.print(timers[i].repeat ? F(" every ") : F(" once "));
      Serial.print(timers[i].intervalMs);
      Serial.print(F("ms "));
      Serial.println(timers[i].command);
    }
    if (!any) Serial.println(F("(no timers)"));
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: timer rm <id>")); return; }
    uint8_t id = args[2].toInt();
    for (uint8_t i = 0; i < MAX_TIMERS; i++) {
      if (timers[i].active && timers[i].id == id) {
        timers[i] = TimerJob();
        saveTimers();
        Serial.println(F("OK"));
        return;
      }
    }
    Serial.println(F("timer: not found"));
  } else if (sub == "clear") {
    for (uint8_t i = 0; i < MAX_TIMERS; i++) timers[i] = TimerJob();
    saveTimers();
    Serial.println(F("OK"));
  } else {
    Serial.println(F("timer: unknown subcommand"));
  }
}

void processTimers() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < MAX_TIMERS; i++) {
    if (!timers[i].active) continue;
    if ((long)(now - timers[i].nextRun) >= 0) {
      timers[i].nextRun = now + timers[i].intervalMs;
      Serial.print(F("[timer "));
      Serial.print(timers[i].id);
      Serial.println(F("]"));
      executeLine(timers[i].command);
      if (!timers[i].repeat) {
        timers[i] = TimerJob();
        saveTimers();
      }
    }
  }
}

uint8_t ruleMetricFromToken(String token) {
  token.toLowerCase();
  if (token == "temp" || token == "temperature" || token == "temperature_c") return 1;
  if (token == "hum" || token == "humidity" || token == "humidity_pct") return 2;
  if (token == "press" || token == "pressure" || token == "pressure_hpa") return 3;
  return 0;
}

String ruleMetricName(uint8_t metric) {
  if (metric == 1) return "temp";
  if (metric == 2) return "hum";
  if (metric == 3) return "press";
  return "?";
}

uint8_t ruleOpFromToken(String token) {
  token.toLowerCase();
  if (token == "gt" || token == ">" || token == "above") return 1;
  if (token == "lt" || token == "<" || token == "below") return 2;
  if (token == "range" || token == "band" || token == "hyst" || token == "hysteresis") return 3;
  if (token == "=" || token == "==" || token == "eq" || token == "is") return 4;
  if (token == "!=" || token == "=!" || token == "<>" || token == "ne" || token == "not") return 5;
  if (token == "<=" || token == "=<" || token == "le") return 6;
  if (token == ">=" || token == "=>" || token == "ge") return 7;
  return 0;
}

String ruleOpName(uint8_t op) {
  if (op == 3) return "range";
  if (op == 1) return "gt";
  if (op == 2) return "lt";
  if (op == 4) return "=";
  if (op == 5) return "!=";
  if (op == 6) return "<=";
  if (op == 7) return ">=";
  return "?";
}

void saveRules() {
  if (!ensureFS()) return;
  String out;
  for (uint8_t i = 0; i < MAX_RULES; i++) {
    if (!rules[i].active) continue;
    out += String(rules[i].id) + ",";
    out += ruleMetricName(rules[i].metric) + ",";
    out += ruleOpName(rules[i].op) + ",";
    out += String(rules[i].threshold, 2) + ",";
    out += String(rules[i].threshold2, 2) + ",";
    out += String(rules[i].cooldownMs) + ",";
    out += rules[i].command + ",";
    out += rules[i].offCommand + "\n";
  }
  writeWholeFile(CONF_RULES, out);
  configSetValue("rule.every", String(ruleIntervalMs));
  configSetValue("rule.cooldown", String(ruleCooldownMs));
}

void loadRules() {
  for (uint8_t i = 0; i < MAX_RULES; i++) rules[i] = Rule();
  nextRuleId = 1;
  ruleIntervalMs = max(1000UL, (unsigned long)configGetValue("rule.every", "5000").toInt());
  ruleCooldownMs = (unsigned long)configGetValue("rule.cooldown", "0").toInt();
  nextRuleEvalMs = millis() + ruleIntervalMs;
  if (!fsReady || !LittleFS.exists(CONF_RULES)) return;
  File file = LittleFS.open(CONF_RULES, "r");
  if (!file) return;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#")) continue;
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    int p4 = line.indexOf(',', p3 + 1);
    int p5 = line.indexOf(',', p4 + 1);
    int p6 = line.indexOf(',', p5 + 1);
    int p7 = line.indexOf(',', p6 + 1);
    if (p1 <= 0 || p2 <= p1 || p3 <= p2 || p4 <= p3) continue;
    for (uint8_t i = 0; i < MAX_RULES; i++) {
      if (rules[i].active) continue;
      rules[i].id = line.substring(0, p1).toInt();
      rules[i].metric = ruleMetricFromToken(line.substring(p1 + 1, p2));
      rules[i].op = ruleOpFromToken(line.substring(p2 + 1, p3));
      rules[i].threshold = line.substring(p3 + 1, p4).toFloat();
      if (p5 > p4 && p6 > p5 && p7 > p6) {
        rules[i].threshold2 = line.substring(p4 + 1, p5).toFloat();
        rules[i].cooldownMs = line.substring(p5 + 1, p6).toInt();
        rules[i].command = line.substring(p6 + 1, p7);
        rules[i].offCommand = line.substring(p7 + 1);
      } else {
        rules[i].threshold2 = 0;
        rules[i].cooldownMs = ruleCooldownMs;
        rules[i].command = line.substring(p4 + 1);
      }
      rules[i].active = rules[i].id > 0 && rules[i].metric > 0 && rules[i].op > 0 && rules[i].command.length() > 0;
      if (rules[i].id >= nextRuleId) nextRuleId = rules[i].id + 1;
      break;
    }
  }
  file.close();
}

String rulesText() {
  String out = "every: " + String(ruleIntervalMs) + "ms\n";
  out += "cooldown: " + String(ruleCooldownMs) + "ms\n";
  bool any = false;
  for (uint8_t i = 0; i < MAX_RULES; i++) {
    if (!rules[i].active) continue;
    any = true;
    out += String(rules[i].id) + " ";
    out += ruleMetricName(rules[i].metric) + " ";
    out += ruleOpName(rules[i].op) + " ";
    out += String(rules[i].threshold, 2);
    if (rules[i].op == 3) out += " " + String(rules[i].threshold2, 2);
    out += " cd=" + String(rules[i].cooldownMs) + " -> ";
    out += rules[i].command + "\n";
    if (rules[i].op == 3 && rules[i].offCommand.length()) out += "    off -> " + rules[i].offCommand + "\n";
  }
  if (!any) out += "(no rules)\n";
  return out;
}

bool ruleMetricValue(const SensorReading& reading, uint8_t metric, float& value) {
  if (!reading.ok) return false;
  if (metric == 1) { value = reading.temperatureC; return true; }
  if (metric == 2 && reading.hasHumidity) { value = reading.humidityPct; return true; }
  if (metric == 3) { value = reading.pressureHpa; return true; }
  return false;
}

void processRules() {
  unsigned long now = millis();
  if ((long)(now - nextRuleEvalMs) < 0) return;
  nextRuleEvalMs = now + ruleIntervalMs;

  bool any = false;
  for (uint8_t i = 0; i < MAX_RULES; i++) if (rules[i].active) { any = true; break; }
  if (!any) return;

  SensorReading reading = readBme();
  if (!reading.ok) return;
  for (uint8_t i = 0; i < MAX_RULES; i++) {
    if (!rules[i].active) continue;
    float value;
    if (!ruleMetricValue(reading, rules[i].metric, value)) continue;
    unsigned long cd = rules[i].cooldownMs ? rules[i].cooldownMs : ruleCooldownMs;
    if (cd && (long)(now - rules[i].lastRunMs) < (long)cd) continue;
    if (rules[i].op == 3) {
      if (!rules[i].latched && value > rules[i].threshold2) {
        rules[i].latched = true;
        rules[i].lastRunMs = now;
        Serial.print(F("[rule "));
        Serial.print(rules[i].id);
        Serial.println(F(" on]"));
        executeLine(rules[i].command);
        yield();
      } else if (rules[i].latched && value < rules[i].threshold && rules[i].offCommand.length()) {
        rules[i].latched = false;
        rules[i].lastRunMs = now;
        Serial.print(F("[rule "));
        Serial.print(rules[i].id);
        Serial.println(F(" off]"));
        executeLine(rules[i].offCommand);
        yield();
      }
      continue;
    }
    bool match = compareFloat(value, rules[i].op == 1 ? 4 : (rules[i].op == 2 ? 3 : (rules[i].op == 4 ? 1 : (rules[i].op == 5 ? 2 : (rules[i].op == 6 ? 5 : 6)))), rules[i].threshold);
    if (match) {
      rules[i].lastRunMs = now;
      Serial.print(F("[rule "));
      Serial.print(rules[i].id);
      Serial.println(F("]"));
      executeLine(rules[i].command);
      yield();
    }
  }
}

void cmdRule(String args[], int argc) {
  if (argc < 2 || args[1] == "list" || args[1] == "status") {
    Serial.print(rulesText());
    return;
  }
  String sub = args[1];
  sub.toLowerCase();
  if (sub == "add") {
    if (argc < 6) { Serial.println(F("usage: rule add temp|hum|press <op> <value> <cmd> | range <low> <high> relay <name>")); return; }
    uint8_t metric = ruleMetricFromToken(args[2]);
    uint8_t op = ruleOpFromToken(args[3]);
    if (!metric) { Serial.println(F("rule: metric must be temp, hum, or press")); return; }
    if (!op) { Serial.println(F("rule: op must be = != < > <= >= gt lt or range")); return; }
    if (op == 3 && argc < 8) { Serial.println(F("usage: rule add <metric> range <low> <high> relay <name>|<on_cmd> ; off optional via rule off")); return; }
    for (uint8_t i = 0; i < MAX_RULES; i++) {
      if (rules[i].active) continue;
      rules[i] = Rule();
      rules[i].id = nextRuleId++;
      rules[i].metric = metric;
      rules[i].op = op;
      rules[i].threshold = args[4].toFloat();
      rules[i].cooldownMs = ruleCooldownMs;
      if (op == 3) {
        rules[i].threshold2 = args[5].toFloat();
        if (rules[i].threshold2 <= rules[i].threshold) { Serial.println(F("rule: high must be greater than low")); return; }
        if (args[6] == "relay" && argc >= 8) {
          rules[i].command = "relay on " + args[7];
          rules[i].offCommand = "relay off " + args[7];
        } else {
          rules[i].command = joinArgs(args, argc, 6);
        }
      } else {
        rules[i].command = joinArgs(args, argc, 5);
      }
      rules[i].active = true;
      saveRules();
      Serial.print(F("rule id "));
      Serial.println(rules[i].id);
      return;
    }
    Serial.println(F("rule: table full"));
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: rule rm <id>")); return; }
    uint8_t id = args[2].toInt();
    for (uint8_t i = 0; i < MAX_RULES; i++) {
      if (rules[i].active && rules[i].id == id) {
        rules[i] = Rule();
        saveRules();
        Serial.println(F("OK"));
        return;
      }
    }
    Serial.println(F("rule: not found"));
  } else if (sub == "clear") {
    for (uint8_t i = 0; i < MAX_RULES; i++) rules[i] = Rule();
    saveRules();
    Serial.println(F("OK"));
  } else if (sub == "every") {
    if (argc < 3) {
      Serial.println(ruleIntervalMs);
      return;
    }
    unsigned long ms = args[2].toInt();
    if (ms < 1000) { Serial.println(F("rule: interval must be >=1000ms")); return; }
    ruleIntervalMs = ms;
    nextRuleEvalMs = millis() + ruleIntervalMs;
    configSetValue("rule.every", String(ruleIntervalMs));
    Serial.println(F("OK"));
  } else if (sub == "cooldown") {
    if (argc < 3) {
      Serial.println(ruleCooldownMs);
      return;
    }
    ruleCooldownMs = max(0L, args[2].toInt());
    configSetValue("rule.cooldown", String(ruleCooldownMs));
    for (uint8_t i = 0; i < MAX_RULES; i++) if (rules[i].active && rules[i].cooldownMs == 0) rules[i].lastRunMs = 0;
    Serial.println(F("OK"));
  } else if (sub == "off") {
    if (argc < 4) { Serial.println(F("usage: rule off <id> <command>")); return; }
    uint8_t id = args[2].toInt();
    for (uint8_t i = 0; i < MAX_RULES; i++) {
      if (rules[i].active && rules[i].id == id) {
        rules[i].offCommand = joinArgs(args, argc, 3);
        saveRules();
        Serial.println(F("OK"));
        return;
      }
    }
    Serial.println(F("rule: not found"));
  } else if (sub == "save") {
    saveRules();
    Serial.println(F("OK"));
  } else if (sub == "load") {
    loadRules();
    Serial.println(F("OK"));
  } else {
    Serial.println(F("rule: unknown subcommand"));
  }
}

bool isUnsignedNumber(String token) {
  token.trim();
  if (!token.length()) return false;
  for (uint8_t i = 0; i < token.length(); i++) {
    if (!isDigit(token[i])) return false;
  }
  return true;
}

bool parseClockTime(const String& token, uint8_t& hour, uint8_t& minute) {
  int colon = token.indexOf(':');
  if (colon <= 0) return false;
  String hourText = token.substring(0, colon);
  String minuteText = token.substring(colon + 1);
  if (!isUnsignedNumber(hourText) || !isUnsignedNumber(minuteText)) return false;
  int h = hourText.toInt();
  int m = minuteText.toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  hour = h;
  minute = m;
  return true;
}

String twoDigits(uint8_t value) {
  return value < 10 ? "0" + String(value) : String(value);
}

String cronModeName(uint8_t mode) {
  if (mode == 1) return "dow";
  if (mode == 2) return "date";
  return "daily";
}

int cronDowFromToken(String token) {
  token.toLowerCase();
  token.trim();
  if (token == "sun" || token == "0" || token == "7") return 0;
  if (token == "mon" || token == "1") return 1;
  if (token == "tue" || token == "2") return 2;
  if (token == "wed" || token == "3") return 3;
  if (token == "thu" || token == "4") return 4;
  if (token == "fri" || token == "5") return 5;
  if (token == "sat" || token == "6") return 6;
  return -1;
}

bool parseDowMask(String spec, uint8_t& mask) {
  mask = 0;
  spec.replace(";", ",");
  int start = 0;
  int specLen = spec.length();
  while (start <= specLen) {
    int comma = spec.indexOf(',', start);
    String token = comma < 0 ? spec.substring(start) : spec.substring(start, comma);
    token.trim();
    if (!token.length()) return false;
    int day = cronDowFromToken(token);
    if (day < 0) return false;
    mask |= (1 << day);
    if (comma < 0) break;
    start = comma + 1;
  }
  return mask != 0;
}

String dowMaskText(uint8_t mask) {
  const char* names[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  String out;
  for (uint8_t i = 0; i < 7; i++) {
    if (!(mask & (1 << i))) continue;
    if (out.length()) out += ",";
    out += names[i];
  }
  return out.length() ? out : "*";
}

int monthFromToken(String token) {
  token.toLowerCase();
  token.trim();
  if (isUnsignedNumber(token)) return token.toInt();
  if (token == "jan" || token == "january") return 1;
  if (token == "feb" || token == "february") return 2;
  if (token == "mar" || token == "march") return 3;
  if (token == "apr" || token == "april") return 4;
  if (token == "may") return 5;
  if (token == "jun" || token == "june") return 6;
  if (token == "jul" || token == "july") return 7;
  if (token == "aug" || token == "august") return 8;
  if (token == "sep" || token == "sept" || token == "september") return 9;
  if (token == "oct" || token == "october") return 10;
  if (token == "nov" || token == "november") return 11;
  if (token == "dec" || token == "december") return 12;
  return 0;
}

bool parseMonthDay(String spec, uint8_t& month, uint8_t& day) {
  spec.toLowerCase();
  spec.replace("/", "-");
  int dash = spec.indexOf('-');
  if (dash <= 0) return false;
  String first = spec.substring(0, dash);
  String second = spec.substring(dash + 1);
  first.trim();
  second.trim();
  int aMonth = monthFromToken(first);
  int bMonth = monthFromToken(second);
  int a = isUnsignedNumber(first) ? first.toInt() : 0;
  int b = isUnsignedNumber(second) ? second.toInt() : 0;
  if (aMonth > 0 && b > 0) {
    month = aMonth;
    day = b;
  } else if (bMonth > 0 && a > 0 && !isUnsignedNumber(second)) {
    day = a;
    month = bMonth;
  } else if (a > 0 && b > 0) {
    if (a > 12 && b <= 12) {
      day = a;
      month = b;
    } else {
      month = a;
      day = b;
    }
  } else {
    return false;
  }
  if (month < 1 || month > 12 || day < 1) return false;
  const uint8_t daysInMonth[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return day <= daysInMonth[month - 1];
}

String cronSpecText(const CronJob& cron) {
  if (cron.mode == 1) return dowMaskText(cron.dowMask);
  if (cron.mode == 2) return twoDigits(cron.month) + "-" + twoDigits(cron.day);
  return "*";
}

String dateText() {
  time_t now = time(nullptr);
  if (now < 1600000000) return "date: time not synced\n";
  struct tm* tm = localtime(&now);
  if (!tm) return "date: unavailable\n";
  char buf[80];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
           tm->tm_hour, tm->tm_min, tm->tm_sec);
  String out = String(buf) + "\n";
  out += "epoch: " + String((unsigned long)now) + "\n";
  return out;
}

void saveCrons() {
  if (!ensureFS()) return;
  String out;
  for (uint8_t i = 0; i < MAX_CRONS; i++) {
    if (!crons[i].active) continue;
    out += String(crons[i].id) + ",";
    out += cronModeName(crons[i].mode) + ",";
    String spec = cronSpecText(crons[i]);
    spec.replace(",", ";");
    out += spec + ",";
    out += twoDigits(crons[i].hour) + ":" + twoDigits(crons[i].minute) + ",";
    out += crons[i].command + "\n";
  }
  writeWholeFile(CONF_CRONS, out);
  configSetValue("cron.every", String(cronIntervalMs));
}

void loadCrons() {
  for (uint8_t i = 0; i < MAX_CRONS; i++) crons[i] = CronJob();
  nextCronId = 1;
  cronIntervalMs = max(10000UL, (unsigned long)configGetValue("cron.every", "30000").toInt());
  nextCronEvalMs = millis() + 2000;
  if (!fsReady || !LittleFS.exists(CONF_CRONS)) return;
  File file = LittleFS.open(CONF_CRONS, "r");
  if (!file) return;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (!line.length() || line.startsWith("#")) continue;
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    int p4 = line.indexOf(',', p3 + 1);
    if (p1 <= 0 || p2 <= p1) continue;
    uint8_t mode = 0;
    uint8_t hour = 0, minute = 0, dowMask = 0, month = 0, day = 0;
    String command;
    if (p3 > p2 && p4 > p3) {
      String modeText = line.substring(p1 + 1, p2);
      modeText.toLowerCase();
      if (modeText == "daily") {
        mode = 0;
      } else if (modeText == "dow") {
        mode = 1;
        if (!parseDowMask(line.substring(p2 + 1, p3), dowMask)) continue;
      } else if (modeText == "date") {
        mode = 2;
        if (!parseMonthDay(line.substring(p2 + 1, p3), month, day)) continue;
      } else {
        continue;
      }
      if (!parseClockTime(line.substring(p3 + 1, p4), hour, minute)) continue;
      command = line.substring(p4 + 1);
    } else {
      if (!parseClockTime(line.substring(p1 + 1, p2), hour, minute)) continue;
      command = line.substring(p2 + 1);
    }
    for (uint8_t i = 0; i < MAX_CRONS; i++) {
      if (crons[i].active) continue;
      crons[i].id = line.substring(0, p1).toInt();
      crons[i].mode = mode;
      crons[i].hour = hour;
      crons[i].minute = minute;
      crons[i].dowMask = dowMask;
      crons[i].month = month;
      crons[i].day = day;
      crons[i].lastRunDay = -1;
      crons[i].command = command;
      crons[i].active = crons[i].id > 0 && crons[i].command.length() > 0;
      if (crons[i].id >= nextCronId) nextCronId = crons[i].id + 1;
      break;
    }
  }
  file.close();
}

String cronsText() {
  String out = "every: " + String(cronIntervalMs) + "ms\n";
  out += dateText();
  bool any = false;
  for (uint8_t i = 0; i < MAX_CRONS; i++) {
    if (!crons[i].active) continue;
    any = true;
    out += String(crons[i].id) + " ";
    out += cronModeName(crons[i].mode) + " ";
    if (crons[i].mode != 0) out += cronSpecText(crons[i]) + " ";
    out += twoDigits(crons[i].hour) + ":" + twoDigits(crons[i].minute) + " -> ";
    out += crons[i].command + "\n";
  }
  if (!any) out += "(no crons)\n";
  return out;
}

void processCrons() {
  unsigned long nowMs = millis();
  if ((long)(nowMs - nextCronEvalMs) < 0) return;
  nextCronEvalMs = nowMs + cronIntervalMs;

  bool any = false;
  for (uint8_t i = 0; i < MAX_CRONS; i++) if (crons[i].active) { any = true; break; }
  if (!any) return;

  time_t now = time(nullptr);
  if (now < 1600000000) return;
  struct tm* tm = localtime(&now);
  if (!tm) return;
  int dayKey = (tm->tm_year * 400) + tm->tm_yday;

  for (uint8_t i = 0; i < MAX_CRONS; i++) {
    if (!crons[i].active) continue;
    bool dateMatch = crons[i].mode == 0 ||
                     (crons[i].mode == 1 && (crons[i].dowMask & (1 << tm->tm_wday))) ||
                     (crons[i].mode == 2 && crons[i].month == (uint8_t)(tm->tm_mon + 1) && crons[i].day == (uint8_t)tm->tm_mday);
    if (dateMatch && crons[i].hour == tm->tm_hour && crons[i].minute == tm->tm_min && crons[i].lastRunDay != dayKey) {
      crons[i].lastRunDay = dayKey;
      Serial.print(F("[cron "));
      Serial.print(crons[i].id);
      Serial.println(F("]"));
      executeLine(crons[i].command);
      yield();
    }
  }
}

void cmdCron(String args[], int argc) {
  if (argc < 2 || args[1] == "list" || args[1] == "status") {
    Serial.print(cronsText());
    return;
  }
  String sub = args[1];
  sub.toLowerCase();
  if (sub == "add") {
    if (argc < 4) {
      Serial.println(F("usage: cron add HH:MM <cmd> | daily HH:MM <cmd> | dow wed,fri HH:MM <cmd> | date 05-01 HH:MM <cmd>"));
      return;
    }
    uint8_t mode = 0;
    uint8_t hour = 0, minute = 0, dowMask = 0, month = 0, day = 0;
    uint8_t timeArg = 2;
    uint8_t commandArg = 3;
    String modeText = args[2];
    modeText.toLowerCase();
    if (modeText == "daily") {
      if (argc < 5) {
        Serial.println(F("usage: cron add daily HH:MM <command>"));
        return;
      }
      timeArg = 3;
      commandArg = 4;
    } else if (modeText == "dow") {
      if (argc < 6) {
        Serial.println(F("usage: cron add dow wed,fri HH:MM <command>"));
        return;
      }
      mode = 1;
      if (!parseDowMask(args[3], dowMask)) {
        Serial.println(F("cron: bad day list; use mon,tue"));
        return;
      }
      timeArg = 4;
      commandArg = 5;
    } else if (modeText == "date") {
      if (argc < 6) {
        Serial.println(F("usage: cron add date 05-01 HH:MM <command>"));
        return;
      }
      mode = 2;
      if (!parseMonthDay(args[3], month, day)) {
        Serial.println(F("cron: bad date; use MM-DD, may-01, or 1-may"));
        return;
      }
      timeArg = 4;
      commandArg = 5;
    }
    if (!parseClockTime(args[timeArg], hour, minute)) { Serial.println(F("cron: bad time; use HH:MM")); return; }
    String command = joinArgs(args, argc, commandArg);
    command.trim();
    if (!command.length()) { Serial.println(F("cron: missing command")); return; }
    for (uint8_t i = 0; i < MAX_CRONS; i++) {
      if (crons[i].active) continue;
      crons[i] = CronJob();
      crons[i].id = nextCronId++;
      crons[i].mode = mode;
      crons[i].hour = hour;
      crons[i].minute = minute;
      crons[i].dowMask = dowMask;
      crons[i].month = month;
      crons[i].day = day;
      crons[i].lastRunDay = -1;
      crons[i].command = command;
      crons[i].active = true;
      saveCrons();
      Serial.print(F("cron id "));
      Serial.println(crons[i].id);
      return;
    }
    Serial.println(F("cron: table full"));
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: cron rm <id>")); return; }
    uint8_t id = args[2].toInt();
    for (uint8_t i = 0; i < MAX_CRONS; i++) {
      if (crons[i].active && crons[i].id == id) {
        crons[i] = CronJob();
        saveCrons();
        Serial.println(F("OK"));
        return;
      }
    }
    Serial.println(F("cron: not found"));
  } else if (sub == "clear") {
    for (uint8_t i = 0; i < MAX_CRONS; i++) crons[i] = CronJob();
    saveCrons();
    Serial.println(F("OK"));
  } else if (sub == "every") {
    if (argc < 3) { Serial.println(cronIntervalMs); return; }
    unsigned long ms = args[2].toInt();
    if (ms < 10000) { Serial.println(F("cron: interval must be >=10000ms")); return; }
    cronIntervalMs = ms;
    nextCronEvalMs = millis() + 1000;
    configSetValue("cron.every", String(cronIntervalMs));
    Serial.println(F("OK"));
  } else if (sub == "save") {
    saveCrons();
    Serial.println(F("OK"));
  } else if (sub == "load") {
    loadCrons();
    Serial.println(F("OK"));
  } else {
    Serial.println(F("cron: unknown subcommand"));
  }
}

void cmdConfig(String args[], int argc) {
  if (!ensureFS()) return;
  if (argc < 2) { Serial.println(F("usage: config get|set|rm|list")); return; }
  String sub = args[1]; sub.toLowerCase();
  if (sub == "list") {
    File file = LittleFS.open(CONF_CONFIG, "r");
    if (!file) { Serial.println(F("(empty)")); return; }
    while (file.available()) Serial.write(file.read());
    file.close();
  } else if (sub == "get") {
    if (argc < 3) { Serial.println(F("usage: config get <key>")); return; }
    Serial.println(configGetValue(args[2], ""));
  } else if (sub == "set") {
    if (argc < 4) { Serial.println(F("usage: config set <key> <value>")); return; }
    Serial.println(configSetValue(args[2], joinArgs(args, argc, 3)) ? F("OK") : F("config: set failed"));
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: config rm <key>")); return; }
    Serial.println(configRemoveValue(args[2]) ? F("OK") : F("config: key not found"));
  } else {
    Serial.println(F("config: unknown subcommand"));
  }
}

void cmdAlias(String args[], int argc) {
  if (argc == 2 && args[1] == "save") { saveAliases(); return; }
  if (argc == 2 && args[1] == "load") { loadAliases(); Serial.println(F("OK")); return; }
  if (argc == 2 && args[1] == "clear") {
    for (uint8_t i = 0; i < MAX_ALIASES; i++) aliases[i] = AliasEntry();
    Serial.println(F("OK"));
    return;
  }
  if (argc == 1) {
    for (uint8_t i = 0; i < MAX_ALIASES; i++) if (aliases[i].active) {
      Serial.print(aliases[i].name); Serial.print(F(" -> ")); Serial.println(aliases[i].command);
    }
    return;
  }
  if (argc < 3) { Serial.println(F("usage: alias [save|load|clear] | alias <name> <command>")); return; }
  for (uint8_t i = 0; i < MAX_ALIASES; i++) {
    if (aliases[i].active && aliases[i].name == args[1]) {
      aliases[i].command = joinArgs(args, argc, 2);
      Serial.println(F("OK"));
      return;
    }
  }
  for (uint8_t i = 0; i < MAX_ALIASES; i++) {
    if (!aliases[i].active) {
      aliases[i].name = args[1];
      aliases[i].command = joinArgs(args, argc, 2);
      aliases[i].active = true;
      Serial.println(F("OK"));
      return;
    }
  }
  Serial.println(F("alias: table full"));
}

void cmdEnv(String args[], int argc, bool setMode, bool unsetMode) {
  if (!setMode && !unsetMode) {
    for (uint8_t i = 0; i < MAX_ENV_VARS; i++) if (envVars[i].active) {
      Serial.print(envVars[i].name); Serial.print('='); Serial.println(envVars[i].value);
    }
    return;
  }
  if (unsetMode) {
    if (argc < 2) { Serial.println(F("usage: unset <name>")); return; }
    for (uint8_t i = 0; i < MAX_ENV_VARS; i++) if (envVars[i].active && envVars[i].name == args[1]) {
      envVars[i] = EnvEntry();
      Serial.println(F("OK"));
      return;
    }
    Serial.println(F("unset: not found"));
    return;
  }
  if (argc < 3) { Serial.println(F("usage: set <name> <value>")); return; }
  for (uint8_t i = 0; i < MAX_ENV_VARS; i++) if (envVars[i].active && envVars[i].name == args[1]) {
    envVars[i].value = joinArgs(args, argc, 2);
    Serial.println(F("OK"));
    return;
  }
  for (uint8_t i = 0; i < MAX_ENV_VARS; i++) if (!envVars[i].active) {
    envVars[i].name = args[1];
    envVars[i].value = joinArgs(args, argc, 2);
    envVars[i].active = true;
    Serial.println(F("OK"));
    return;
  }
  Serial.println(F("set: env table full"));
}

String resetReason() {
  return ESP.getResetReason();
}

void cmdSysinfo(const String& cmd) {
  if (cmd == "mem") {
    Serial.print(F("free heap: ")); Serial.println(ESP.getFreeHeap());
    Serial.print(F("frag: ")); Serial.print(ESP.getHeapFragmentation()); Serial.println('%');
    Serial.print(F("max block: ")); Serial.println(ESP.getMaxFreeBlockSize());
  } else if (cmd == "resetreason") {
    Serial.println(resetReason());
  } else if (cmd == "chip") {
    Serial.print(F("chip id: ")); Serial.println(ESP.getChipId(), HEX);
    Serial.print(F("flash id: ")); Serial.println(ESP.getFlashChipId(), HEX);
    Serial.print(F("sdk: ")); Serial.println(ESP.getSdkVersion());
  } else if (cmd == "flash") {
    Serial.print(F("real size: ")); Serial.println(ESP.getFlashChipRealSize());
    Serial.print(F("ide size: ")); Serial.println(ESP.getFlashChipSize());
    Serial.print(F("speed: ")); Serial.println(ESP.getFlashChipSpeed());
  } else {
    Serial.println(F(KERNEL_NAME " " KERNEL_VERSION));
    cmdSysinfo("chip");
    cmdSysinfo("flash");
    cmdSysinfo("mem");
    Serial.print(F("reset: ")); Serial.println(resetReason());
  }
}

void cmdLog(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "show";
  sub.toLowerCase();
  if (sub == "clear") {
    if (ensureFS()) LittleFS.remove(LOG_FILE);
    Serial.println(F("OK"));
  } else if (sub == "save") {
    appendPersistentLog(argc >= 3 ? joinArgs(args, argc, 2) : "manual log save");
    Serial.println(F("OK"));
  } else if (sub == "compact") {
    compactLogIfNeeded();
    Serial.println(F("OK"));
  } else if (sub == "flash") {
    if (argc < 3) {
      Serial.println(configGetValue("log.persist", "off"));
      return;
    }
    String value = args[2]; value.toLowerCase();
    if (value != "on" && value != "off") { Serial.println(F("usage: log flash on|off")); return; }
    Serial.println(configSetValue("log.persist", value) ? F("OK") : F("log: config failed"));
  } else if (sub == "status") {
    if (!ensureFS()) return;
    File file = LittleFS.open(LOG_FILE, "r");
    Serial.print(F("log bytes: "));
    Serial.println(file ? file.size() : 0);
    if (file) file.close();
    Serial.print(F("max bytes: "));
    Serial.println(LOG_MAX_BYTES);
    Serial.print(F("flash: "));
    Serial.println(configGetValue("log.persist", "off"));
  } else if (sub == "tail" || sub == "head") {
    String toolArgs[4];
    toolArgs[0] = sub;
    toolArgs[1] = "-n";
    toolArgs[2] = argc >= 3 ? args[2] : "10";
    toolArgs[3] = LOG_FILE;
    if (sub == "tail") cmdTail(toolArgs, 4);
    else cmdHead(toolArgs, 4);
  } else {
    if (!ensureFS()) return;
    File file = LittleFS.open(LOG_FILE, "r");
    if (!file) { Serial.println(F("(empty)")); return; }
    while (file.available()) Serial.write(file.read());
    file.close();
  }
}

void cmdLogger(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: logger [-p level] <message>")); return; }
  uint8_t start = 1;
  String level = "info";
  if (argc >= 4 && args[1] == "-p") {
    level = args[2];
    start = 3;
  }
  String msg = level + ": " + joinArgs(args, argc, start);
  addLogLine(msg);
  appendPersistentLog(msg);
  Serial.println(F("OK"));
}

String htmlEscape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

String urlEscape(const String& s) {
  String out;
  const char* hex = "0123456789ABCDEF";
  for (uint16_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += c;
    else {
      out += '%';
      out += hex[(c >> 4) & 0x0F];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "\\r");
  s.replace("\t", "\\t");
  return s;
}

String webKey() {
  String key = configGetValue("web.key", "admin");
  key.trim();
  return key.length() ? key : "admin";
}

bool webLockoutEnabled() {
  return configGetValue("web.lockout", "on") != "off";
}

bool webAuthLocked() {
  return webLockoutEnabled() && webAuthLockedUntil && (long)(webAuthLockedUntil - millis()) > 0;
}

void webAuthResetFailures() {
  webAuthFails = 0;
  webAuthLockedUntil = 0;
}

void webAuthRegisterFailure() {
  if (!webLockoutEnabled()) return;
  uint8_t maxFails = (uint8_t)configGetValue("web.lockout.max", "5").toInt();
  if (maxFails < 2) maxFails = 2;
  if (maxFails > 20) maxFails = 20;
  if (webAuthFails < 250) webAuthFails++;
  if (webAuthFails >= maxFails) {
    unsigned long lockMs = (unsigned long)configGetValue("web.lockout.ms", "300000").toInt();
    if (lockMs < 10000UL) lockMs = 10000UL;
    if (lockMs > 3600000UL) lockMs = 3600000UL;
    webAuthLockedUntil = millis() + lockMs;
    webAuthFails = 0;
    eventLog("web auth locked");
  }
}

bool webCookieOk() {
  String cookie = webServer.header("Cookie");
  String token = "KESP=" + webKey();
  int pos = cookie.indexOf(token);
  while (pos >= 0) {
    int end = pos + token.length();
    bool starts = pos == 0 || cookie[pos - 1] == ';' || cookie[pos - 1] == ' ';
    bool ends = end >= (int)cookie.length() || cookie[end] == ';';
    if (starts && ends) return true;
    pos = cookie.indexOf(token, pos + 1);
  }
  return false;
}

String webPropagatedKey(const String& keyArg) {
  if (!keyArg.length()) return "";
  if (webCookieOk() || keyArg == webKey()) return "";
  return keyArg;
}

String authQuery() {
  String keyArg = webPropagatedKey(webServer.arg("key"));
  return keyArg.length() ? "?key=" + urlEscape(keyArg) : "";
}

String authParamPrefix() {
  String keyArg = webPropagatedKey(webServer.arg("key"));
  return keyArg.length() ? "key=" + urlEscape(keyArg) + "&" : "";
}

bool webAuthOk() {
  if (webCookieOk()) return true;
  if (webServer.arg("key") == webKey()) {
    webAuthResetFailures();
    webServer.sendHeader("Set-Cookie", "KESP=" + webKey() + "; Path=/; SameSite=Strict");
    return true;
  }
  if (webAuthLocked()) {
    webServer.send(429, "text/plain", "web auth locked; wait and retry");
    return false;
  }
  bool badKey = webServer.hasArg("key") && webServer.arg("key").length();
  if (badKey) webAuthRegisterFailure();
  String html = F("<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width'><title>KernelESP Login</title>");
  html += F("<link rel='stylesheet' href='/style.css'><style>");
  html += F(".loginWrap{min-height:100vh;display:grid;place-items:center;padding:18px;background:#eef2f6}");
  html += F(".loginBox{width:min(560px,100%);background:#fff;border:1px solid #d6dde7;border-radius:8px;box-shadow:0 16px 36px rgba(16,24,40,.12);overflow:hidden}");
  html += F(".loginTop{background:#111820;color:#f7fafc;padding:18px 20px;border-bottom:4px solid #0d6b7d}.loginTop h1{margin:0;font-size:24px}.loginTop p{margin:5px 0 0;color:#c9d6e2}");
  html += F(".loginBody{padding:18px 20px}.loginGrid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;margin:12px 0}.loginItem{border:1px solid #d6dde7;border-radius:7px;background:#f8fafc;padding:9px}.loginItem span{display:block;color:#667085;font-size:12px}.loginItem strong{display:block;overflow-wrap:anywhere}");
  html += F(".loginNote{border-radius:7px;padding:9px 10px;margin:10px 0;background:#ebf2ff;color:#244f96}.loginWarn{background:#fff2e8;color:#8a3f1c}.loginOk{background:#e7f6ef;color:#0f684d}.loginForm{display:block}.loginForm label{display:block;font-weight:700;margin:12px 0 6px}.loginForm button{width:100%;margin:10px 0 0}.loginHelp{border-top:1px solid #d6dde7;margin-top:14px;padding-top:12px;color:#667085;font-size:13px}");
  html += F("@media(max-width:520px){.loginGrid{grid-template-columns:1fr}.loginTop,.loginBody{padding:16px}}");
  html += F("</style></head><body><div class='loginWrap'><section class='loginBox'><div class='loginTop'><h1>KernelESP</h1><p>Local microcontroller console</p></div><div class='loginBody'>");
  if (badKey) html += F("<div class='loginNote loginWarn'>Incorrect key. Check the web key or use the serial console to change it.</div>");
  else if (webKey() == "admin") html += F("<div class='loginNote loginWarn'>The web key is still the factory default. Change it after setup.</div>");
  else html += F("<div class='loginNote loginOk'>Access is protected. Enter the web key to continue.</div>");
  html += F("<div class='loginGrid'><div class='loginItem'><span>Firmware</span><strong>");
  html += F(KERNEL_VERSION);
  html += F("</strong></div><div class='loginItem'><span>Wi-Fi</span><strong>");
  html += WiFi.status() == WL_CONNECTED ? F("connected") : (wifiConnecting ? F("connecting") : F("offline"));
  html += F("</strong></div><div class='loginItem'><span>IP</span><strong>");
  html += WiFi.localIP().toString();
  html += F("</strong></div><div class='loginItem'><span>SSID</span><strong>");
  html += htmlEscape(WiFi.SSID().length() ? WiFi.SSID() : String("(no SSID)"));
  html += F("</strong></div></div>");
  html += F("<form class='loginForm' method='POST' action='/login'><input name='next' type='hidden' value='");
  html += htmlEscape(webServer.uri());
  html += F("'><label for='key'>Web key</label><input id='key' name='key' type='password' placeholder='Enter the access key' autocomplete='current-password' autofocus><button>Sign in</button></form>");
  html += F("<div class='loginHelp'>Serial recovery: <code>config set web.key new_key</code>. If the network fails, use <code>wifi status</code> or <code>wifi recover</code>.</div>");
  html += F("</div></section></div><script src='/i18n-es.js?v=19'></script><script src='/i18n-es2.js?v=19'></script><script src='/i18n-es3.js?v=19'></script><script src='/i18n-es4.js?v=19'></script><script src='/i18n-es5.js?v=19'></script><script src='/i18n-pt.js?v=19'></script><script src='/i18n-pt2.js?v=19'></script><script src='/i18n-pt3.js?v=19'></script><script src='/i18n-pt4.js?v=19'></script><script src='/i18n-pt5.js?v=19'></script><script src='/i18n.js?v=19'></script></body></html>");
  webServer.send(401, "text/html; charset=utf-8", html);
  return false;
}

String webHeader(const String& title, const String& keyArg) {
  String navKey = webPropagatedKey(keyArg);
  String query = navKey.length() ? "?key=" + urlEscape(navKey) : "";
  String html = F("<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width'><title>");
  html += htmlEscape(title);
  html += F("</title>");
  html += F("<link rel='stylesheet' href='/style.css'>");
  html += F("</head><body><header><h1>KernelESP</h1><nav><a href='/");
  html += query;
  html += F("'>Dashboard</a><a href='/ui");
  html += query;
  html += F("'>Live UI</a><a href='/edit");
  html += query;
  html += F("'>Editor</a><a href='/automations");
  html += query;
  html += F("'>Auto</a><a href='/wizard");
  html += query;
  html += F("'>Wizard</a><a href='/diag");
  html += query;
  html += F("'>Diag</a><a href='/profiles");
  html += query;
  html += F("'>Profiles</a><a href='/help");
  html += query;
  html += F("'>Help</a><a href='/relays");
  html += query;
  html += F("'>Relays</a><a href='/logs");
  html += query;
  html += F("'>Logs</a><a href='/settings");
  html += query;
  html += F("'>Settings</a></nav></header><main>");
  return html;
}

String webFooter() {
  return F("</main><script src='/i18n-es.js?v=19'></script><script src='/i18n-es2.js?v=19'></script><script src='/i18n-es3.js?v=19'></script><script src='/i18n-es4.js?v=19'></script><script src='/i18n-es5.js?v=19'></script><script src='/i18n-pt.js?v=19'></script><script src='/i18n-pt2.js?v=19'></script><script src='/i18n-pt3.js?v=19'></script><script src='/i18n-pt4.js?v=19'></script><script src='/i18n-pt5.js?v=19'></script><script src='/i18n.js?v=19'></script></body></html>");
}

String fsListToString(const String& rawPath) {
  if (!fsReady) return "LittleFS not mounted\n";
  String path = rawPath.length() ? normalizePath(rawPath) : cwd;
  if (path == "/proc") return "meminfo\nuptime\nwifi\nrelays\nversion\nfilesystems\nflash\n";
  if (!pathExists(path)) return "ls: not found\n";
  String out;
  if (!isDirectory(path)) {
    File file = LittleFS.open(path, "r");
    if (!file) return "ls: cannot open file\n";
    out += basenameOf(path) + "\t" + String(file.size()) + "B\n";
    file.close();
    return out;
  }
  Dir dir = LittleFS.openDir(path);
  bool empty = true;
  while (dir.next()) {
    empty = false;
    out += basenameOf(dir.fileName());
    if (dir.isDirectory()) out += "/";
    else out += "\t" + String(dir.fileSize()) + "B";
    out += "\n";
  }
  if (empty) out = "(empty)\n";
  return out;
}

String fileRowsHtml(const String& dirPath, const String& keyArg) {
  String out;
  if (!fsReady) return F("<p class='muted'>LittleFS not mounted</p>");
  Dir dir = LittleFS.openDir(dirPath);
  while (dir.next()) {
    if (dir.isDirectory()) continue;
    String path = dir.fileName();
    out += F("<div class='row'><span><strong>");
    out += htmlEscape(basenameOf(path));
    out += F("</strong> <span class='pill'>");
    out += String(dir.fileSize());
    out += F("B</span></span><span><a class='btn secondary' href='/edit");
    out += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=" : "?path=";
    out += urlEscape(path);
    out += F("'>Edit</a><a class='btn secondary' href='/run");
    out += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=" : "?path=";
    out += urlEscape(path);
    out += F("'>Run</a><a class='btn warn' href='/delete");
    out += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=" : "?path=";
    out += urlEscape(path);
    out += F("'>Delete</a></span></div>");
  }
  return out.length() ? out : F("<p class='muted'>(empty)</p>");
}

String relayStatusText() {
  String out;
  for (uint8_t i = 0; i < MAX_RELAYS; i++) {
    if (!relays[i].configured) continue;
    out += relays[i].name + " GPIO" + String(relays[i].pin);
    out += relays[i].activeLow ? " active_low " : " active_high ";
    out += relays[i].state ? "ON boot=" : "OFF boot=";
    out += relays[i].bootState + "\n";
  }
  return out.length() ? out : "(no relays)\n";
}

String sensorText() {
  SensorReading r = readBme();
  if (!r.ok) return "sensor: not found\n";
  String out = "type: ";
  out += r.hasHumidity ? "BME280\n" : "BMP280\n";
  out += "temperature_c: " + String(r.temperatureC, 2) + "\n";
  out += "pressure_hpa: " + String(r.pressureHpa, 2) + "\n";
  if (r.hasHumidity) out += "humidity_pct: " + String(r.humidityPct, 2) + "\n";
  return out;
}

String sensorJson() {
  SensorReading r = readBme();
  if (!r.ok) return "{\"ok\":false}";
  String out = "{\"ok\":true,\"type\":\"";
  out += r.hasHumidity ? "BME280" : "BMP280";
  out += "\",\"temperature_c\":" + String(r.temperatureC, 2);
  out += ",\"pressure_hpa\":" + String(r.pressureHpa, 2);
  if (r.hasHumidity) out += ",\"humidity_pct\":" + String(r.humidityPct, 2);
  out += "}";
  return out;
}

String captureOutputForLine(const String& command) {
  String output;
  StringCapture capture(output, 7000);
  Serial.capture(&capture);
  executeLine(command);
  Serial.releaseCapture();
  if (!output.length()) output = "(no output)\n";
  else if (!output.endsWith("\n")) output += "\n";
  if (capture.truncated()) output += "\n[output truncated]\n";
  return output;
}

String captureOutputForScript(const String& path) {
  String output;
  StringCapture capture(output, 7000);
  Serial.capture(&capture);
  runScriptFile(path);
  Serial.releaseCapture();
  if (!output.length()) output = "(no output)\n";
  else if (!output.endsWith("\n")) output += "\n";
  if (capture.truncated()) output += "\n[output truncated]\n";
  return output;
}

String captureOnlyOutputForLine(const String& command) {
  String output;
  StringCapture capture(output, PIPE_CAPTURE_BYTES);
  bool oldSuppressHistory = suppressHistory;
  suppressHistory = true;
  Serial.capture(&capture, true);
  executeLine(command);
  Serial.releaseCapture();
  suppressHistory = oldSuppressHistory;
  if (!output.length()) output = "";
  else if (!output.endsWith("\n")) output += "\n";
  if (capture.truncated()) output += "\n[pipe input truncated]\n";
  return output;
}

int findPipeOutsideQuotes(const String& line) {
  bool quoted = false;
  char quoteChar = '\0';
  for (uint16_t i = 0; i < line.length(); i++) {
    char c = line[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
    } else if (c == '|' && !quoted && (i + 1 >= line.length() || line[i + 1] != '|') && (i == 0 || line[i - 1] != '|')) {
      return i;
    }
  }
  return -1;
}

bool hasPipeOutsideQuotes(const String& line) {
  return findPipeOutsideQuotes(line) >= 0;
}

int splitPipeline(String line, String parts[], int maxParts) {
  int count = 0;
  bool quoted = false;
  char quoteChar = '\0';
  String current;
  line.trim();
  for (uint16_t i = 0; i < line.length(); i++) {
    char c = line[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
      current += c;
    } else if (c == '|' && !quoted && (i + 1 >= line.length() || line[i + 1] != '|') && (i == 0 || line[i - 1] != '|')) {
      current.trim();
      if (!current.length()) return -1;
      if (count >= maxParts) return -2;
      parts[count++] = current;
      current = "";
    } else {
      current += c;
    }
  }
  current.trim();
  if (!current.length()) return -1;
  if (count >= maxParts) return -2;
  parts[count++] = current;
  return count;
}

int pipeLineLimit(String args[], int argc, int fallback) {
  int limit = fallback;
  if (argc >= 3 && args[1] == "-n") limit = args[2].toInt();
  else if (argc >= 2 && args[1].startsWith("-")) limit = args[1].substring(1).toInt();
  if (limit < 1) limit = fallback;
  if (limit > 40) limit = 40;
  return limit;
}

String pipeGrep(const String& input, const String& pattern) {
  String out;
  int pos = 0;
  while (pos <= (int)input.length()) {
    int nl = input.indexOf('\n', pos);
    String line = nl >= 0 ? input.substring(pos, nl) : input.substring(pos);
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    if (line.indexOf(pattern) >= 0) out += line + "\n";
    if (nl < 0) break;
    pos = nl + 1;
    yield();
  }
  return out;
}

String pipeHead(const String& input, int limit) {
  String out;
  int pos = 0, printed = 0;
  while (pos <= (int)input.length() && printed < limit) {
    int nl = input.indexOf('\n', pos);
    String line = nl >= 0 ? input.substring(pos, nl) : input.substring(pos);
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    out += line + "\n";
    printed++;
    if (nl < 0) break;
    pos = nl + 1;
    yield();
  }
  return out;
}

String pipeTail(const String& input, int limit) {
  String ring[40];
  int count = 0, pos = 0;
  while (pos <= (int)input.length()) {
    int nl = input.indexOf('\n', pos);
    String line = nl >= 0 ? input.substring(pos, nl) : input.substring(pos);
    if (line.endsWith("\r")) line.remove(line.length() - 1);
    if (line.length() || nl >= 0) ring[count % limit] = line;
    count++;
    if (nl < 0) break;
    pos = nl + 1;
    yield();
  }
  String out;
  int start = count > limit ? count - limit : 0;
  for (int i = start; i < count; i++) {
    out += ring[i % limit] + "\n";
    yield();
  }
  return out;
}

String pipeWc(const String& input, String args[], int argc) {
  uint32_t bytes = input.length(), lines = 0, words = 0;
  bool inWord = false;
  for (uint16_t i = 0; i < input.length(); i++) {
    char c = input[i];
    if (c == '\n') lines++;
    if (isSpace(c)) inWord = false;
    else if (!inWord) {
      words++;
      inWord = true;
    }
    yield();
  }
  if (argc >= 2 && args[1] == "-l") return String(lines) + "\n";
  if (argc >= 2 && args[1] == "-w") return String(words) + "\n";
  if (argc >= 2 && args[1] == "-c") return String(bytes) + "\n";
  return String(lines) + " " + String(words) + " " + String(bytes) + "\n";
}

String applyPipeStage(const String& stage, const String& input, bool& handled) {
  handled = true;
  String args[MAX_ARGS];
  int argc = splitArgs(stage, args, MAX_ARGS);
  if (argc == 0) return input;
  String cmd = args[0];
  cmd.toLowerCase();
  if (cmd == "cat" && argc == 1) return input;
  if (cmd == "grep") {
    if (argc < 2) return "usage: grep <text>\n";
    return pipeGrep(input, args[1]);
  }
  if (cmd == "head") return pipeHead(input, pipeLineLimit(args, argc, 10));
  if (cmd == "tail") return pipeTail(input, pipeLineLimit(args, argc, 10));
  if (cmd == "wc") return pipeWc(input, args, argc);
  if (cmd == "tee") {
    if (argc < 2) return "usage: tee <file>\n";
    if (!ensureFS()) return input;
    String path = normalizePath(args[1]);
    if (!parentDirectoryExists(path) || isDirectory(path)) return "tee: cannot write file\n";
    File file = LittleFS.open(path, "w");
    if (!file) return "tee: cannot write file\n";
    file.print(input);
    file.close();
    return input;
  }
  handled = false;
  return "";
}

String pipelineOutput(String line) {
  String parts[MAX_PIPE_STAGES];
  int count = splitPipeline(line, parts, MAX_PIPE_STAGES);
  if (count == -1) return "pipe: empty stage\n";
  if (count == -2) return "pipe: too many stages\n";
  if (count < 2) return captureOnlyOutputForLine(line);
  String data = captureOnlyOutputForLine(parts[0]);
  for (int i = 1; i < count; i++) {
    bool handled = false;
    String next = applyPipeStage(parts[i], data, handled);
    if (!handled) return "pipe: unsupported stage '" + parts[i] + "'\n";
    data = next;
  }
  return data;
}

void executePipeline(String line) {
  String output = pipelineOutput(line);
  if (!output.length()) return;
  Serial.print(output);
  if (!output.endsWith("\n")) Serial.println();
}

String webCommandOutput(String command) {
  command.trim();
  String args[MAX_ARGS];
  int argc = splitArgs(command, args, MAX_ARGS);
  if (argc == 0) return "";
  if (hasPipeOutsideQuotes(command)) return pipelineOutput(command);
  String cmd = args[0];
  cmd.toLowerCase();

  if (cmd == "ls") return fsListToString(argc > 1 ? args[1] : "");
  if (cmd == "pwd") return cwd + "\n";
  if (cmd == "cat") {
    if (argc < 2) return "usage: cat <file>\n";
    String path = normalizePath(args[1]);
    if (path.startsWith("/proc/")) {
      String out = procText(path);
      return out.length() ? out : "cat: cannot open file\n";
    }
    File file = LittleFS.open(path, "r");
    if (!file || file.isDirectory()) return "cat: cannot open file\n";
    String out = file.readString();
    file.close();
    if (!out.endsWith("\n")) out += "\n";
    return out;
  }
  if (cmd == "free" || cmd == "heap" || cmd == "mem") {
    return "free heap: " + String(ESP.getFreeHeap()) + "\nheap frag: " +
           String(ESP.getHeapFragmentation()) + "%\nmax block: " +
           String(ESP.getMaxFreeBlockSize()) + "\n";
  }
  if (cmd == "df") {
    if (!fsReady) return "LittleFS not mounted\n";
    FSInfo info;
    LittleFS.info(info);
    return "total: " + String(info.totalBytes) + "\nused:  " + String(info.usedBytes) +
           "\nfree:  " + String(info.totalBytes - info.usedBytes) + "\n";
  }
  if (cmd == "version") return String(KERNEL_NAME) + " " + KERNEL_VERSION + "\n";
  if (cmd == "health") return healthText();
  if (cmd == "uname" || cmd == "sysinfo") {
    return String(KERNEL_NAME) + " " + KERNEL_VERSION + "\narch: esp8266\nip: " +
           WiFi.localIP().toString() + "\n";
  }
  if (cmd == "wifi") {
    if (argc < 2 || args[1] == "status") {
      return wifiStatusText();
    }
    if (args[1] == "net") return wifiNetText();
    if (args[1] == "ip") return WiFi.localIP().toString() + "\n";
    if (args[1] == "mac") return WiFi.macAddress() + "\n";
  }
  if (cmd == "armed") return String(automationsArmed ? "on\n" : "off\n");
  if (cmd == "relay") {
    if (argc < 2 || args[1] == "status") {
      return relayStatusText();
    }
  }
  if (cmd == "rule") {
    if (argc < 2 || args[1] == "list" || args[1] == "status") return rulesText();
  }
  if (cmd == "cron") {
    if (argc < 2 || args[1] == "list" || args[1] == "status") return cronsText();
  }
  if (cmd == "input") {
    if (argc < 2 || args[1] == "list" || args[1] == "status") return inputsText();
  }
  if (cmd == "scene") {
    if (argc < 2 || args[1] == "list") {
      String out = readWholeFile(CONF_SCENES);
      return out.length() ? out : "(no scenes)\n";
    }
  }
  if (cmd == "state") {
    if (argc < 2 || args[1] == "list") {
      String out = readWholeFile(CONF_STATE);
      return out.length() ? out : "(empty)\n";
    }
  }
  if (cmd == "date") return dateText();
  if (cmd == "config") {
    if (argc < 2 || args[1] == "list") return readWholeFile(CONF_CONFIG);
    if (args[1] == "get" && argc >= 3) return configGetValue(args[2], "") + "\n";
  }
  if (cmd == "log") {
    if (argc >= 2 && (args[1] == "status" || args[1] == "compact" || args[1] == "clear" || args[1] == "save")) {
      return captureOutputForLine(command);
    }
    String out = readWholeFile(LOG_FILE);
    return out.length() ? out : "(empty)\n";
  }
  if (cmd == "pins") {
    String out = "Safe-ish NodeMCU pins: D1 D2 D5 D6 D7\n";
    out += "Boot/serial pins: D0 D3 D4 D8 RX TX LED\n";
    for (uint8_t i = 0; i < sizeof(PIN_ALIASES) / sizeof(PIN_ALIASES[0]); i++) {
      out += String(PIN_ALIASES[i].name) + "=GPIO" + String(PIN_ALIASES[i].gpio);
      if (PIN_ALIASES[i].risky) out += " *";
      out += "\n";
    }
    return out;
  }
  if (cmd == "sensor") {
    if (argc < 2 || args[1] == "read") return sensorText();
  }
  if (cmd == "time" || cmd == "ntp") {
    if (argc < 2 || args[1] == "status" || args[1] == "show") return timeStatusText();
  }

  return captureOutputForLine(command);
}

void handleWebRoot() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String html = webHeader("KernelESP", keyArg);
  html += F("<div class='grid'><section class='card'><h2>Status</h2><p><span class='pill'>IP ");
  html += WiFi.localIP().toString();
  html += F("</span> <span class='pill'>Heap ");
  html += String(ESP.getFreeHeap());
  html += F("</span></p><p class='muted'>Wi-Fi: ");
  html += htmlEscape(WiFi.SSID());
  html += F("</p><p><a class='btn secondary' href='/");
  html += keyArg.length() ? "?key=" + urlEscape(keyArg) : "";
  html += F("'>Refresh</a> <a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=free'>Memory</a> <a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=wifi%20status'>Wi-Fi</a> <a class='btn secondary' href='/api/status");
  html += authQuery();
  html += F("'>API</a></p></section>");
  html += F("<section class='card'><h2>Command</h2><form action='/cmd'><input name='c' placeholder='ls /' autofocus><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'> <button>Run</button></form><p><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=ls%20/'>ls /</a> <a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=relay%20status'>relay status</a> <a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=timer%20list'>timers</a> <a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=rule%20list'>rules</a> <a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=cron%20list'>cron</a></p></section></div>");
  html += F("<div class='grid'><section class='card'><h2>Time</h2><pre>");
  html += htmlEscape(timeStatusText());
  html += F("</pre><p><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=ntp%20kick'>Kick NTP</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=date'>Date</a></p></section><section class='card'><h2>Sensor</h2><pre>");
  html += htmlEscape(sensorText());
  html += F("</pre><p><a class='btn secondary' href='/api/sensor");
  html += authQuery();
  html += F("'>Sensor API</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=i2c%20scan'>I2C scan</a></p></section></div>");
  html += F("<section class='card'><h2>Relays</h2>");
  bool anyRelay = false;
  for (uint8_t i = 0; i < MAX_RELAYS; i++) {
    if (!relays[i].configured) continue;
    anyRelay = true;
    html += F("<div class='relay'><div><strong>");
    html += htmlEscape(relays[i].name);
    html += F("</strong> <span class='pill'>GPIO");
    html += String(relays[i].pin);
    html += F("</span> ");
    html += relays[i].state ? F("<span class='pill'>ON</span>") : F("<span class='pill'>OFF</span>");
    html += F("</div><div><a class='btn ok' href='/relay?");
    html += authParamPrefix();
    html += F("name=");
    html += htmlEscape(relays[i].name);
    html += F("&state=on'>ON</a><a class='btn warn' href='/relay?");
    html += authParamPrefix();
    html += F("name=");
    html += htmlEscape(relays[i].name);
    html += F("&state=off'>OFF</a><a class='btn secondary' href='/relay?");
    html += authParamPrefix();
    html += F("name=");
    html += htmlEscape(relays[i].name);
    html += F("&state=toggle'>TOGGLE</a><a class='btn secondary' href='/relay?");
    html += authParamPrefix();
    html += F("name=");
    html += htmlEscape(relays[i].name);
    html += F("&state=pulse&ms=500'>PULSE</a></div></div>");
  }
  if (!anyRelay) html += F("<p class='muted'>No relays configured. Use serial or command: <code>relay add light D1 active_low</code></p>");
  html += F("</section><div class='grid'><section class='card'><h2>Scripts</h2><p><a class='btn' href='/edit");
  html += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=/boot.sh" : "?path=/boot.sh";
  html += F("'>Edit /boot.sh</a> <a class='btn secondary' href='/edit");
  html += authQuery();
  html += F("'>Open editor</a></p>");
  html += fileRowsHtml("/", keyArg);
  html += F("</section><section class='card'><h2>Tools</h2><p><a class='btn secondary' href='/logs");
  html += authQuery();
  html += F("'>Logs</a><a class='btn secondary' href='/settings");
  html += authQuery();
  html += F("'>Settings</a><a class='btn secondary' href='/automations");
  html += authQuery();
  html += F("'>Automations</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=health'>Health</a><a class='btn secondary' href='/restore");
  html += authQuery();
  html += F("'>Restore</a><a class='btn secondary' href='/backup");
  html += authQuery();
  html += F("'>Backup</a></p></section></div>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebRelay() {
  if (!webAuthOk()) return;
  String name = webServer.arg("name");
  String state = webServer.arg("state");
  int idx = findRelay(name);
  if (idx < 0) { webServer.send(404, "text/plain", "relay not found"); return; }
  bool changed = false;
  if (state == "pulse") pulseRelay(idx, webServer.arg("ms").toInt());
  else if (state == "toggle") changed = applyRelay(idx, !relays[idx].state);
  else changed = applyRelay(idx, state == "on");
  if (changed) saveRelays();
  webServer.sendHeader("Location", "/" + authQuery());
  webServer.send(303);
}

void handleWebRelaysPage() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String html = webHeader("KernelESP Relays", keyArg);
  html += F("<section class='card'><h2>Relays</h2><pre>");
  html += htmlEscape(relayStatusText());
  html += F("</pre>");
  for (uint8_t i = 0; i < MAX_RELAYS; i++) {
    if (!relays[i].configured) continue;
    html += F("<div class='relay'><div><strong>");
    html += htmlEscape(relays[i].name);
    html += F("</strong> GPIO");
    html += String(relays[i].pin);
    html += relays[i].activeLow ? F(" active_low ") : F(" active_high ");
    html += relays[i].state ? F("ON") : F("OFF");
    html += F(" boot=");
    html += htmlEscape(relays[i].bootState);
    html += F("</div><div><a class='btn ok' href='/relay?");
    html += authParamPrefix();
    html += F("name=");
    html += urlEscape(relays[i].name);
    html += F("&state=on'>ON</a><a class='btn warn' href='/relay?");
    html += authParamPrefix();
    html += F("name=");
    html += urlEscape(relays[i].name);
    html += F("&state=off'>OFF</a><a class='btn secondary' href='/relay?");
    html += authParamPrefix();
    html += F("name=");
    html += urlEscape(relays[i].name);
    html += F("&state=toggle'>TOGGLE</a><a class='btn secondary' href='/relay?");
    html += authParamPrefix();
    html += F("name=");
    html += urlEscape(relays[i].name);
    html += F("&state=pulse&ms=500'>PULSE</a></div></div>");
  }
  html += F("<h2>Add relay</h2><form action='/cmd'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='c' value='relay add light D1 active_low'><button>Run</button></form></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebAutomations() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String html = webHeader("KernelESP Automations", keyArg);
  html += F("<div class='grid'><section class='card'><h2>Jobs</h2><pre>");
  html += htmlEscape(captureOutputForLine("jobs"));
  html += F("</pre></section><section class='card'><h2>Inputs</h2><pre>");
  html += htmlEscape(inputsText());
  html += F("</pre></section></div><div class='grid'><section class='card'><h2>Scenes</h2><pre>");
  String scenes = readWholeFile(CONF_SCENES);
  html += htmlEscape(scenes.length() ? scenes : "(no scenes)\n");
  html += F("</pre></section><section class='card'><h2>State</h2><pre>");
  String state = readWholeFile(CONF_STATE);
  html += htmlEscape(state.length() ? state : "(empty)\n");
  html += F("</pre></section></div><section class='card'><h2>Command</h2><form action='/cmd'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='c' placeholder='cron add daily 08:00 relay on pump'><button>Run</button></form><p>");
  html += F("<a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=cron%20list'>Cron</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=rule%20list'>Rules</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=timer%20list'>Timers</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=scene%20list'>Scenes</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=input%20list'>Inputs</a></p></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebCmd() {
  if (!webAuthOk()) return;
  String command = webServer.arg("c");
  String output = webCommandOutput(command);
  String keyArg = webServer.arg("key");
  String html = webHeader("KernelESP Command", keyArg);
  html += F("<section class='card'><h2>Command</h2><form action='/cmd'><input name='c' value='");
  html += htmlEscape(command);
  html += F("' autofocus><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><button>Run</button></form><pre>");
  html += htmlEscape(output);
  html += F("</pre><p><a class='btn secondary' href='/?key=");
  html += urlEscape(keyArg);
  html += F("'>Home</a></p></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebEdit() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String path = webServer.arg("path");
  if (!path.length()) path = "/boot.sh";
  path = normalizePath(path);
  String content = readWholeFile(path);
  String html = webHeader("KernelESP Editor", keyArg);
  html += F("<div class='grid'><section class='card'><h2>Files</h2>");
  html += fileRowsHtml("/", keyArg);
  html += F("<form method='GET' action='/edit'><p><input name='path' placeholder='/new.sh'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'> <button>New/Open</button></p></form></section><section class='card'><h2>Quick Commands</h2><p><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=boot%20show'>boot show</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=timer%20list'>timer list</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=relay%20status'>relay status</a></p></section></div>");
  html += F("<section class='card'><h2>Editor</h2><form method='POST' action='/save'><p><input name='path' value='");
  html += htmlEscape(path);
  html += F("'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'> <button>Save</button> <button name='run' value='1'>Save & Run</button></p><textarea name='content' spellcheck='false'>");
  html += htmlEscape(content);
  html += F("</textarea></form><p><a class='btn secondary' href='/run");
  html += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=" : "?path=";
  html += urlEscape(path);
  html += F("'>Run</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=sh%20-n%20");
  html += urlEscape(path);
  html += F("'>Validate</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=boot%20set%20");
  html += urlEscape(path);
  html += F("'>Set boot</a><a class='btn warn' href='/delete");
  html += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=" : "?path=";
  html += urlEscape(path);
  html += F("'>Delete</a></p><p class='muted'>Tip: scripts use one command per line. Example: <code>relay pulse light 500</code></p></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebSave() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String path = normalizePath(webServer.arg("path"));
  String content = webServer.arg("content");
  bool ok = writeWholeFile(path, content);
  String html = webHeader("KernelESP Save", keyArg);
  html += F("<section class='card'><h2>Save</h2><pre>");
  html += ok ? "Saved " + htmlEscape(path) : "Save failed";
  html += F("</pre><p><a class='btn' href='/edit?key=");
  html += urlEscape(keyArg);
  html += F("&path=");
  html += urlEscape(path);
  html += F("'>Back to editor</a> <a class='btn secondary' href='/?key=");
  html += urlEscape(keyArg);
  html += F("'>Home</a></p>");
  if (ok && webServer.arg("run") == "1") {
    html += F("<h2>Run</h2><pre>");
    html += htmlEscape("Running " + path + "\n");
    html += htmlEscape(captureOutputForScript(path));
    html += F("</pre>");
  }
  html += F("</section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebRun() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String path = normalizePath(webServer.arg("path"));
  String html = webHeader("KernelESP Run", keyArg);
  html += F("<section class='card'><h2>Run ");
  html += htmlEscape(path);
  html += F("</h2><pre>");
  html += htmlEscape(captureOutputForScript(path));
  html += F("</pre><p><a class='btn' href='/edit");
  html += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=" : "?path=";
  html += urlEscape(path);
  html += F("'>Back to editor</a></p></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebDelete() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String path = normalizePath(webServer.arg("path"));
  bool confirmed = webServer.arg("confirm") == "yes";
  bool ok = confirmed && path != "/" && !isDirectory(path) && LittleFS.remove(path);
  String html = webHeader("KernelESP Delete", keyArg);
  html += F("<section class='card'><h2>Delete</h2><pre>");
  html += ok ? "Deleted " + htmlEscape(path) : (confirmed ? "Delete failed" : "Confirm delete");
  html += F("</pre>");
  if (!confirmed) {
    html += F("<p><a class='btn warn' href='/delete");
    html += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=" : "?path=";
    html += urlEscape(path);
    html += F("&confirm=yes'>Delete ");
    html += htmlEscape(path);
    html += F("</a></p>");
  }
  html += F("<p><a class='btn' href='/edit");
  html += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&path=" : "?path=";
  html += urlEscape(path);
  html += F("'>Back to editor</a></p></section>");
  html += webFooter();
  webServer.send(ok || !confirmed ? 200 : 400, "text/html", html);
}

void handleWebLogs() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  if (webServer.arg("clear") == "1") {
    if (fsReady) LittleFS.remove(LOG_FILE);
  }
  String html = webHeader("KernelESP Logs", keyArg);
  html += F("<section class='card'><h2>dmesg</h2><pre>");
  html += htmlEscape(captureOutputForLine("dmesg"));
  html += F("</pre></section><section class='card'><h2>Persistent log</h2><pre>");
  String log = readWholeFile(LOG_FILE);
  html += htmlEscape(log.length() ? log : "(empty)\n");
  html += F("</pre><p><a class='btn warn' href='/logs");
  html += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&clear=1" : "?clear=1";
  html += F("'>Clear</a></p></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebStyle() {
  if (!fsReady || !LittleFS.exists("/www/style.css")) {
    webServer.send(404, "text/plain", "style not found");
    return;
  }
  File file = LittleFS.open("/www/style.css", "r");
  webServer.streamFile(file, "text/css; charset=utf-8");
  file.close();
}

String contentTypeForPath(const String& path) {
  if (path.endsWith(".css")) return "text/css; charset=utf-8";
  if (path.endsWith(".js")) return "application/javascript; charset=utf-8";
  if (path.endsWith(".json")) return "application/json; charset=utf-8";
  if (path.endsWith(".html")) return "text/html; charset=utf-8";
  if (path.endsWith(".txt") || path.endsWith(".log")) return "text/plain; charset=utf-8";
  if (path.endsWith(".svg")) return "image/svg+xml";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".ico")) return "image/x-icon";
  return "application/octet-stream";
}

void handleWebNotFound() {
  if (!fsReady) { webServer.send(404, "text/plain", "not found"); return; }
  String uri = webServer.uri();
  if (uri.indexOf("..") >= 0) { webServer.send(403, "text/plain", "forbidden"); return; }
  String path = uri.startsWith("/www/") ? uri : "/www" + uri;
  if (!LittleFS.exists(path) || isDirectory(path)) {
    webServer.send(404, "text/plain", "not found");
    return;
  }
  File file = LittleFS.open(path, "r");
  webServer.streamFile(file, contentTypeForPath(path));
  file.close();
}

void handleWebUi() {
  if (!webAuthOk()) return;
  if (!fsReady || !LittleFS.exists("/www/index.html")) {
    webServer.send(404, "text/plain", "ui not installed: missing /www/index.html");
    return;
  }
  File file = LittleFS.open("/www/index.html", "r");
    webServer.streamFile(file, "text/html; charset=utf-8");
  file.close();
}

void handleWebSettings() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  if (webServer.method() == HTTP_POST) {
    if (webServer.arg("newkey").length()) {
      configSetValue("web.key", webServer.arg("newkey"));
      keyArg = webServer.arg("newkey");
      webServer.sendHeader("Set-Cookie", "KESP=" + keyArg + "; Path=/; SameSite=Strict");
    }
    if (webServer.hasArg("web.lockout")) configSetValue("web.lockout", webServer.arg("web.lockout"));
    if (webServer.hasArg("web.lockout.max")) configSetValue("web.lockout.max", webServer.arg("web.lockout.max"));
    if (webServer.hasArg("web.lockout.ms")) configSetValue("web.lockout.ms", webServer.arg("web.lockout.ms"));
    if (webServer.hasArg("web.autostart")) configSetValue("web.autostart", webServer.arg("web.autostart"));
    if (webServer.hasArg("boot.safe")) configSetValue("boot.safe", webServer.arg("boot.safe"));
    if (webServer.hasArg("system.armed")) {
      String armed = webServer.arg("system.armed");
      configSetValue("system.armed", armed);
      automationsArmed = armed == "on";
    }
    if (webServer.hasArg("fallback.ap")) configSetValue("fallback.ap", webServer.arg("fallback.ap"));
    if (webServer.hasArg("wifi.watchdog")) configSetValue("wifi.watchdog", webServer.arg("wifi.watchdog"));
    if (webServer.hasArg("wifi.reboot_on_fail")) configSetValue("wifi.reboot_on_fail", webServer.arg("wifi.reboot_on_fail"));
    if (webServer.hasArg("ap.ssid")) configSetValue("ap.ssid", webServer.arg("ap.ssid"));
    if (webServer.hasArg("ap.key")) configSetValue("ap.key", webServer.arg("ap.key"));
    if (webServer.hasArg("ntp.autosync")) configSetValue("ntp.autosync", webServer.arg("ntp.autosync"));
    if (webServer.hasArg("ntp.server1")) configSetValue("ntp.server1", webServer.arg("ntp.server1"));
    if (webServer.hasArg("ntp.server2")) configSetValue("ntp.server2", webServer.arg("ntp.server2"));
    if (webServer.hasArg("ntp.tz")) configSetValue("ntp.tz", webServer.arg("ntp.tz"));
    if (webServer.hasArg("sensor.autostart")) configSetValue("sensor.autostart", webServer.arg("sensor.autostart"));
    if (webServer.hasArg("sensor.addr")) configSetValue("sensor.addr", webServer.arg("sensor.addr"));
    if (webServer.hasArg("sensor.sda")) configSetValue("sensor.sda", webServer.arg("sensor.sda"));
    if (webServer.hasArg("sensor.scl")) configSetValue("sensor.scl", webServer.arg("sensor.scl"));
  }
  String html = webHeader("KernelESP Settings", keyArg);
  html += F("<section class='card'><h2>Security</h2><form method='POST' action='/settings'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><p><input name='newkey' type='password' placeholder='new web key'> <button>Change key</button></p></form><form method='POST' action='/settings'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><p><select name='web.lockout'><option value='on'");
  if (configGetValue("web.lockout", "on") == "on") html += F(" selected");
  html += F(">lockout on</option><option value='off'");
  if (configGetValue("web.lockout", "on") == "off") html += F(" selected");
  html += F(">lockout off</option></select><input name='web.lockout.max' value='");
  html += htmlEscape(configGetValue("web.lockout.max", "5"));
  html += F("' placeholder='max failures'><input name='web.lockout.ms' value='");
  html += htmlEscape(configGetValue("web.lockout.ms", "300000"));
  html += F("' placeholder='lock ms'><button>Save security</button></p></form></section>");
  html += F("<section class='card'><h2>Boot</h2><form method='POST' action='/settings'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><p><select name='web.autostart'><option value='on'");
  if (configGetValue("web.autostart", "off") == "on") html += F(" selected");
  html += F(">web autostart on</option><option value='off'");
  if (configGetValue("web.autostart", "off") != "on") html += F(" selected");
  html += F(">web autostart off</option></select> <select name='boot.safe'><option value='off'");
  if (configGetValue("boot.safe", "on") != "on") html += F(" selected");
  html += F(">normal boot</option><option value='on'");
  if (configGetValue("boot.safe", "on") == "on") html += F(" selected");
  html += F(">safe boot</option></select> <select name='system.armed'><option value='on'");
  if (automationsArmed) html += F(" selected");
  html += F(">automations armed</option><option value='off'");
  if (!automationsArmed) html += F(" selected");
  html += F(">automations paused</option></select> <button>Save boot settings</button></p></form>");
  html += F("<p><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=safe%20next'>Safe next boot</a><a class='btn secondary' href='/backup");
  html += authQuery();
  html += F("'>Download backup</a></p></section>");
  html += F("<section class='card'><h2>Wi-Fi Recovery</h2><form method='POST' action='/settings'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><p><select name='wifi.watchdog'><option value='on'");
  if (configGetValue("wifi.watchdog", "on") == "on") html += F(" selected");
  html += F(">watchdog on</option><option value='off'");
  if (configGetValue("wifi.watchdog", "on") != "on") html += F(" selected");
  html += F(">watchdog off</option></select> <select name='fallback.ap'><option value='on'");
  if (configGetValue("fallback.ap", "on") == "on") html += F(" selected");
  html += F(">fallback AP on</option><option value='off'");
  if (configGetValue("fallback.ap", "on") != "on") html += F(" selected");
  html += F(">fallback AP off</option></select> <select name='wifi.reboot_on_fail'><option value='off'");
  if (configGetValue("wifi.reboot_on_fail", "off") != "on") html += F(" selected");
  html += F(">no auto reboot</option><option value='on'");
  if (configGetValue("wifi.reboot_on_fail", "off") == "on") html += F(" selected");
  html += F(">reboot on repeated fail</option></select></p><p><input name='ap.ssid' value='");
  html += htmlEscape(configGetValue("ap.ssid", "KernelESP-Setup"));
  html += F("'><input name='ap.key' value='");
  html += htmlEscape(configGetValue("ap.key", ""));
  html += F("' placeholder='AP key min 8 chars, optional'></p><p><button>Save Wi-Fi recovery</button><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=ap%20status'>AP status</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=wifi%20reconnect'>Reconnect</a></p></form></section>");
  html += F("<section class='card'><h2>NTP</h2><form method='POST' action='/settings'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><p><select name='ntp.autosync'><option value='on'");
  if (configGetValue("ntp.autosync", "on") == "on") html += F(" selected");
  html += F(">ntp autosync on</option><option value='off'");
  if (configGetValue("ntp.autosync", "on") != "on") html += F(" selected");
  html += F(">ntp autosync off</option></select></p><p><input name='ntp.server1' value='");
  html += htmlEscape(configGetValue("ntp.server1", "pool.ntp.org"));
  html += F("'><input name='ntp.server2' value='");
  html += htmlEscape(configGetValue("ntp.server2", "time.nist.gov"));
  html += F("'><input name='ntp.tz' value='");
  html += htmlEscape(configGetValue("ntp.tz", "0"));
  html += F("' placeholder='timezone hours, e.g. 1'></p><p><button>Save NTP</button><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=ntp%20kick'>Kick now</a></p></form></section>");
  html += F("<section class='card'><h2>Sensor</h2><form method='POST' action='/settings'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><p><select name='sensor.autostart'><option value='on'");
  if (configGetValue("sensor.autostart", "off") == "on") html += F(" selected");
  html += F(">sensor autostart on</option><option value='off'");
  if (configGetValue("sensor.autostart", "off") != "on") html += F(" selected");
  html += F(">sensor autostart off</option></select></p><p><input name='sensor.addr' value='");
  html += htmlEscape(configGetValue("sensor.addr", "0x76"));
  html += F("'><input name='sensor.sda' value='");
  html += htmlEscape(configGetValue("sensor.sda", "4"));
  html += F("'><input name='sensor.scl' value='");
  html += htmlEscape(configGetValue("sensor.scl", "5"));
  html += F("'></p><p><button>Save sensor</button><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=sensor%20begin'>Begin</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=sensor%20read'>Read</a></p></form></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebLogin() {
  if (webServer.arg("key") == webKey()) {
    String next = webServer.arg("next");
    if (!next.startsWith("/") || next.indexOf("//") >= 0) next = "/";
    webAuthResetFailures();
    webServer.sendHeader("Set-Cookie", "KESP=" + webKey() + "; Path=/; SameSite=Strict");
    webServer.sendHeader("Location", next);
    webServer.send(303);
    return;
  }
  webAuthOk();
}

void appendBackupFile(String& out, const String& path) {
  if (!LittleFS.exists(path) || isDirectory(path)) return;
  String content = readWholeFile(path);
  out += "\n---FILE " + path + " " + String(content.length()) + "---\n";
  out += content;
  if (!out.endsWith("\n")) out += "\n";
}

void appendBackupDir(String& out, const String& dirPath) {
  if (!isDirectory(dirPath)) return;
  Dir dir = LittleFS.openDir(dirPath);
  while (dir.next()) {
    if (!dir.isDirectory()) appendBackupFile(out, dirChildPath(dirPath, dir.fileName()));
    yield();
  }
}

void appendBackupDirRecursive(String& out, const String& dirPath, uint8_t depth) {
  if (depth > 4 || !isDirectory(dirPath)) return;
  Dir dir = LittleFS.openDir(dirPath);
  while (dir.next()) {
    String child = dirChildPath(dirPath, dir.fileName());
    if (dir.isDirectory()) appendBackupDirRecursive(out, child, depth + 1);
    else appendBackupFile(out, child);
    yield();
  }
}

String backupText(bool includeProfiles) {
  String out = "# KernelESP backup " + String(KERNEL_VERSION) + "\n";
  appendBackupDirRecursive(out, "/etc", 0);
  appendBackupDirRecursive(out, "/home", 0);
  appendBackupDirRecursive(out, PKG_DIR, 0);
  if (includeProfiles) appendBackupDirRecursive(out, PROFILE_DIR, 0);
  appendBackupDir(out, "/");
  appendBackupFile(out, LOG_FILE);
  return out;
}

String backupText() {
  return backupText(true);
}

bool restoreBackupText(String content, uint8_t& restored) {
  restored = 0;
  int pos = 0;
  int contentLen = content.length();
  while (pos < contentLen) {
    int marker = content.indexOf("---FILE ", pos);
    if (marker < 0) break;
    int headerEnd = content.indexOf('\n', marker);
    if (headerEnd < 0) break;
    String header = content.substring(marker + 8, headerEnd);
    header.trim();
    int sp = header.lastIndexOf(' ');
    if (sp <= 0) break;
    String path = normalizePath(header.substring(0, sp));
    int size = header.substring(sp + 1).toInt();
    int dataStart = headerEnd + 1;
    if (size < 0 || dataStart + size > contentLen) break;
    String body = content.substring(dataStart, dataStart + size);
    if (path != "/" && parentDirectoryExists(path) && writeWholeFile(path, body)) restored++;
    pos = dataStart + size;
    yield();
  }
  return restored > 0;
}

void cmdRestore(String args[], int argc) {
  if (!ensureFS()) return;
  if (argc < 2) { Serial.println(F("usage: restore <backup-file> --yes")); return; }
  if (argc < 3 || args[2] != "--yes") {
    Serial.println(F("restore: use --yes to confirm overwrite"));
    return;
  }
  String content = readWholeFile(normalizePath(args[1]));
  if (!content.length()) { Serial.println(F("restore: cannot read backup")); return; }
  uint8_t restored = 0;
  if (!restoreBackupText(content, restored)) { Serial.println(F("restore: no files restored")); return; }
  Serial.print(F("restored: "));
  Serial.println(restored);
}

bool copyFilePath(const String& srcPath, const String& dstPath) {
  if (!LittleFS.exists(srcPath) || isDirectory(srcPath) || isDirectory(dstPath)) return false;
  if (!parentDirectoryExists(dstPath)) return false;
  File src = LittleFS.open(srcPath, "r");
  if (!src) return false;
  File dst = LittleFS.open(dstPath, "w");
  if (!dst) { src.close(); return false; }
  uint8_t buffer[64];
  bool ok = true;
  while (src.available()) {
    size_t n = src.read(buffer, sizeof(buffer));
    if (dst.write(buffer, n) != n) { ok = false; break; }
    yield();
  }
  src.close();
  dst.close();
  return ok;
}

bool copyDirPath(const String& srcPath, const String& dstPath, uint8_t depth) {
  if (depth > 5 || !isDirectory(srcPath)) return false;
  if (!pathExists(dstPath) && !LittleFS.mkdir(dstPath)) return false;
  Dir dir = LittleFS.openDir(srcPath);
  bool ok = true;
  while (dir.next()) {
    String child = dirChildPath(srcPath, dir.fileName());
    String target = dstPath + (dstPath.endsWith("/") ? "" : "/") + basenameOf(child);
    if (dir.isDirectory()) ok = copyDirPath(child, target, depth + 1) && ok;
    else ok = copyFilePath(child, target) && ok;
    yield();
  }
  return ok;
}

void sendBackupFile(const String& path) {
  if (!LittleFS.exists(path) || isDirectory(path)) return;
  File file = LittleFS.open(path, "r");
  if (!file) return;
  webServer.sendContent("\n---FILE " + path + " " + String(file.size()) + "---\n");
  uint8_t buffer[96];
  while (file.available()) {
    size_t n = file.read(buffer, sizeof(buffer));
    webServer.sendContent(reinterpret_cast<const char*>(buffer), n);
    yield();
  }
  webServer.sendContent("\n");
  file.close();
}

void sendBackupDir(const String& dirPath) {
  if (!isDirectory(dirPath)) return;
  Dir dir = LittleFS.openDir(dirPath);
  while (dir.next()) {
    if (!dir.isDirectory()) sendBackupFile(dirChildPath(dirPath, dir.fileName()));
    yield();
  }
}

void sendBackupDirRecursive(const String& dirPath, uint8_t depth) {
  if (depth > 4 || !isDirectory(dirPath)) return;
  Dir dir = LittleFS.openDir(dirPath);
  while (dir.next()) {
    String child = dirChildPath(dirPath, dir.fileName());
    if (dir.isDirectory()) sendBackupDirRecursive(child, depth + 1);
    else sendBackupFile(child);
    yield();
  }
}

void handleWebBackup() {
  if (!webAuthOk()) return;
  webServer.sendHeader("Content-Disposition", "attachment; filename=kernelesp-backup.txt");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/plain", "");
  webServer.sendContent("# KernelESP backup " + String(KERNEL_VERSION) + "\n");
  sendBackupDirRecursive("/etc", 0);
  sendBackupDirRecursive("/home", 0);
  sendBackupDirRecursive(PKG_DIR, 0);
  sendBackupDirRecursive(PROFILE_DIR, 0);
  sendBackupDir("/");
  sendBackupFile(LOG_FILE);
  webServer.sendContent("");
}

void handleWebRestore() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String html = webHeader("KernelESP Restore", keyArg);
  if (webServer.method() == HTTP_POST) {
    uint8_t restored = 0;
    bool confirmed = webServer.arg("confirm") == "yes";
    bool ok = confirmed && restoreBackupText(webServer.arg("content"), restored);
    html += F("<section class='card'><h2>Restore</h2><pre>");
    html += ok ? "restored: " + String(restored) : (confirmed ? "restore failed" : "restore: confirmation required");
    html += F("</pre></section>");
  }
  html += F("<section class='card'><h2>Restore Backup</h2><form method='POST' action='/restore'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><textarea name='content' spellcheck='false' placeholder='Paste KernelESP backup here'></textarea><p><label><input type='checkbox' name='confirm' value='yes'> Confirm restore</label> <button>Restore</button></p></form></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleApiStatus() {
  if (!webAuthOk()) return;
  FSInfo info;
  if (fsReady) LittleFS.info(info);
  String out = "{";
  out += "\"name\":\"" KERNEL_NAME "\",\"version\":\"" KERNEL_VERSION "\",";
  out += "\"board\":\"" + jsonEscape(configGetValue("board.profile", "generic")) + "\",";
  out += "\"config_schema\":\"" + jsonEscape(configGetValue("system.config_schema", "0")) + "\",";
  out += "\"web_lockout\":\"" + String(webAuthLocked() ? "locked" : configGetValue("web.lockout", "on")) + "\",";
  out += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  out += "\"heap_frag\":" + String(ESP.getHeapFragmentation()) + ",";
  out += "\"max_block\":" + String(ESP.getMaxFreeBlockSize()) + ",";
  out += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  out += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
  out += "\"mask\":\"" + WiFi.subnetMask().toString() + "\",";
  out += "\"dns1\":\"" + WiFi.dnsIP(0).toString() + "\",";
  out += "\"dns2\":\"" + WiFi.dnsIP(1).toString() + "\",";
  out += "\"dhcp\":\"" + configGetValue("wifi.dhcp", "on") + "\",";
  out += "\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\",";
  out += "\"wifi\":\"" + String(WiFi.status() == WL_CONNECTED ? "connected" : (wifiConnecting ? "connecting" : "not connected")) + "\",";
  out += "\"ap\":\"" + String(fallbackApRunning ? WiFi.softAPIP().toString() : "off") + "\",";
  out += "\"armed\":\"" + String(automationsArmed ? "on" : "off") + "\",";
  out += "\"epoch\":" + String((unsigned long)time(nullptr)) + ",";
  uint8_t ruleCount = 0;
  for (uint8_t i = 0; i < MAX_RULES; i++) if (rules[i].active) ruleCount++;
  out += "\"rules\":" + String(ruleCount) + ",";
  uint8_t cronCount = 0;
  for (uint8_t i = 0; i < MAX_CRONS; i++) if (crons[i].active) cronCount++;
  out += "\"crons\":" + String(cronCount) + ",";
  uint8_t inputCount = 0;
  for (uint8_t i = 0; i < MAX_INPUTS; i++) if (inputs[i].active) inputCount++;
  out += "\"inputs\":" + String(inputCount) + ",";
  out += "\"fs_total\":" + String(fsReady ? info.totalBytes : 0) + ",";
  out += "\"fs_free\":" + String(fsReady ? info.totalBytes - info.usedBytes : 0) + ",";
  out += "\"relays\":[";
  bool first = true;
  for (uint8_t i = 0; i < MAX_RELAYS; i++) {
    if (!relays[i].configured) continue;
    if (!first) out += ",";
    first = false;
    out += "{\"name\":\"" + jsonEscape(relays[i].name) + "\",\"pin\":" + String(relays[i].pin);
    out += ",\"state\":" + String(relays[i].state ? "true" : "false") + "}";
  }
  out += "]}";
  webServer.send(200, "application/json", out);
}

void handleApiSensor() {
  if (!webAuthOk()) return;
  webServer.send(200, "application/json", sensorJson());
}

void handleApiCmd() {
  if (!webAuthOk()) return;
  String command = webServer.arg("c");
  String output = webCommandOutput(command);
  String out = "{\"ok\":true,\"cmd\":\"" + jsonEscape(command) + "\",\"output\":\"" + jsonEscape(output) + "\"}";
  webServer.send(200, "application/json", out);
}

void handleApiRelay() {
  if (!webAuthOk()) return;
  int idx = findRelay(webServer.arg("name"));
  if (idx < 0) { webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"relay not found\"}"); return; }
  String state = webServer.arg("state");
  bool changed = false;
  if (state == "pulse") pulseRelay(idx, webServer.arg("ms").toInt());
  else if (state == "toggle") changed = applyRelay(idx, !relays[idx].state);
  else if (state == "on" || state == "off") changed = applyRelay(idx, state == "on");
  else { webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad state\"}"); return; }
  if (changed) saveRelays();
  webServer.send(200, "application/json", "{\"ok\":true,\"state\":" + String(relays[idx].state ? "true" : "false") + "}");
}

void handleWebDiag() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String html = webHeader("KernelESP Diagnostics", keyArg);
  html += F("<div class='grid'><section class='card'><h2>Health</h2><pre>");
  html += htmlEscape(healthText());
  html += F("</pre><p><a class='btn secondary' href='/diag");
  html += authQuery();
  html += F("'>Refresh</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=wifi%20reconnect'>Wi-Fi reconnect</a><a class='btn secondary' href='/cmd?");
  html += authParamPrefix();
  html += F("c=ap%20start'>AP start</a><a class='btn warn' href='/cmd?");
  html += authParamPrefix();
  html += F("c=reboot'>Reboot</a></p></section><section class='card'><h2>Kernel Log</h2><pre>");
  html += htmlEscape(captureOutputForLine("dmesg"));
  html += F("</pre></section><section class='card'><h2>Wi-Fi</h2><pre>");
  html += htmlEscape(captureOutputForLine("diag wifi"));
  html += F("</pre></section></div>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebHelp() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String topic = webServer.arg("topic");
  topic.trim();
  if (!topic.length()) topic = "index";
  topic.replace("/", "");
  String lang = webServer.arg("lang");
  lang.toLowerCase();
  if (lang != "es" && lang != "pt") lang = "en";
  String path = lang == "en" ? "/help/" + topic + ".txt" : "/help/" + topic + "." + lang + ".txt";
  String text = readWholeFile(path);
  if (!text.length() && lang != "en") text = readWholeFile("/help/" + topic + ".txt");
  if (!text.length()) text = "No local help for topic: " + topic + "\n";
  String html = webHeader("KernelESP Help", keyArg);
  html += F("<section class='card'><h2>Help</h2><p>");
  const char* topics[] = {"index", "quickstart", "hardware", "relay", "sensor", "climate", "cron", "email", "mail", "inputs", "scripts", "web", "wifi", "files", "backup", "memory", "commands", "safety", "troubleshooting"};
  for (uint8_t i = 0; i < sizeof(topics) / sizeof(topics[0]); i++) {
    html += F("<a class='btn secondary' href='/help");
    html += keyArg.length() ? "?key=" + urlEscape(keyArg) + "&topic=" : "?topic=";
    html += topics[i];
    if (lang != "en") {
      html += F("&lang=");
      html += lang;
    }
    html += F("'>");
    html += topics[i];
    html += F("</a>");
  }
  html += F("</p><pre>");
  html += htmlEscape(text);
  html += F("</pre></section>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebProfiles() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String result;
  String editWifiName;
  String editWifiText;
  if (webServer.method() == HTTP_POST) {
    String action = webServer.arg("action");
    String name = webServer.arg("name");
    name.trim();
    String wifiName = webServer.arg("wifi.name");
    wifiName.trim();
    String safeWifiName = safeName(wifiName);
    if (action == "profile_save" && name.length()) {
      result = "$ profile save " + name + "\n" + captureOutputForLine("profile save " + name);
    } else if (action == "profile_load" && name.length() && webServer.arg("confirm") == "yes") {
      result = "$ profile load " + name + " --yes\n" + captureOutputForLine("profile load " + name + " --yes");
    } else if (action == "profile_rm" && name.length() && webServer.arg("confirm") == "yes") {
      result = "$ profile rm " + name + "\n" + captureOutputForLine("profile rm " + name);
    } else if (action == "wifi_save" && safeWifiName.length()) {
      result = "$ wifi profile save " + wifiName + "\n" + captureOutputForLine("wifi profile save " + wifiName);
    } else if (action == "wifi_use" && safeWifiName.length()) {
      result = "$ wifi profile use " + wifiName + "\n" + captureOutputForLine("wifi profile use " + wifiName);
    } else if (action == "wifi_reconnect" && safeWifiName.length()) {
      result = "$ wifi profile use " + wifiName + " reconnect\n" + captureOutputForLine("wifi profile use " + wifiName + " reconnect");
    } else if (action == "wifi_rm" && safeWifiName.length() && webServer.arg("wifi.confirm") == "yes") {
      result = "$ wifi profile rm " + wifiName + "\n" + captureOutputForLine("wifi profile rm " + wifiName);
    } else if (action == "wifi_edit" && safeWifiName.length()) {
      editWifiName = safeWifiName;
      editWifiText = readWholeFile(wifiProfilePath(editWifiName));
      if (!editWifiText.length()) result = "wifi profile: not found\n";
    } else if ((action == "wifi_write" || action == "wifi_write_use") && safeWifiName.length()) {
      LittleFS.mkdir(WIFI_PROFILE_DIR);
      String oldText = readWholeFile(wifiProfilePath(safeWifiName));
      String ssid = webServer.arg("wifi.ssid");
      ssid.trim();
      if (!ssid.length()) {
        result = "wifi profile: SSID is required\n";
      } else {
        String pass = webServer.arg("wifi.password");
        if (!pass.length() && oldText.length()) pass = keyValueFromText(oldText, "password", "");
        String text;
        text += "ssid=" + ssid + "\n";
        text += "password=" + pass + "\n";
        text += "channel=" + (webServer.arg("wifi.channel").length() ? webServer.arg("wifi.channel") : "0") + "\n";
        text += "phy=" + (webServer.arg("wifi.phy").length() ? webServer.arg("wifi.phy") : "11g") + "\n";
        text += "power.dbm=" + (webServer.arg("wifi.power.dbm").length() ? webServer.arg("wifi.power.dbm") : "17.5") + "\n";
        text += "dhcp=" + (webServer.arg("wifi.dhcp").length() ? webServer.arg("wifi.dhcp") : "on") + "\n";
        text += "ip=" + webServer.arg("wifi.ip") + "\n";
        text += "gateway=" + webServer.arg("wifi.gateway") + "\n";
        text += "mask=" + webServer.arg("wifi.mask") + "\n";
        text += "dns1=" + webServer.arg("wifi.dns1") + "\n";
        text += "dns2=" + webServer.arg("wifi.dns2") + "\n";
        bool ok = writeWholeFile(wifiProfilePath(safeWifiName), text);
        result = ok ? "wifi profile saved: " + safeWifiName + "\n" : "wifi profile: save failed\n";
        if (ok && action == "wifi_write_use") result += captureOutputForLine("wifi profile use " + safeWifiName + " reconnect");
        editWifiName = safeWifiName;
        editWifiText = text;
      }
    }
  }
  if (!editWifiName.length() && webServer.hasArg("editwifi")) {
    editWifiName = safeName(webServer.arg("editwifi"));
    editWifiText = readWholeFile(wifiProfilePath(editWifiName));
  }
  String editSsid = keyValueFromText(editWifiText, "ssid", "");
  String editChannel = keyValueFromText(editWifiText, "channel", "0");
  String editPhy = keyValueFromText(editWifiText, "phy", "11g");
  String editPower = keyValueFromText(editWifiText, "power.dbm", "17.5");
  String editDhcp = keyValueFromText(editWifiText, "dhcp", "on");
  String editIp = keyValueFromText(editWifiText, "ip", "");
  String editGateway = keyValueFromText(editWifiText, "gateway", "");
  String editMask = keyValueFromText(editWifiText, "mask", "");
  String editDns1 = keyValueFromText(editWifiText, "dns1", "");
  String editDns2 = keyValueFromText(editWifiText, "dns2", "");
  String html = webHeader("KernelESP Profiles", keyArg);
  html += F("<div class='grid'><section class='card'><h2>System Profiles</h2><p class='muted'>Full system configuration snapshots. These are not Wi-Fi networks.</p><pre>");
  html += htmlEscape(captureOutputForLine("profile list"));
  html += F("</pre><form method='POST' action='/profiles'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='name' placeholder='system profile name'><button name='action' value='profile_save'>Save snapshot</button> <label><input type='checkbox' name='confirm' value='yes'> Confirm load/remove snapshot</label> <button name='action' value='profile_load'>Load snapshot</button> <button name='action' value='profile_rm'>Remove snapshot</button></form></section>");
  html += F("<section class='card'><h2>Wi-Fi Profiles</h2><p class='muted'>Click a profile to load it into the editor. Passwords are not shown here.</p><div class='stack'>");
  LittleFS.mkdir(WIFI_PROFILE_DIR);
  Dir wifiDir = LittleFS.openDir(WIFI_PROFILE_DIR);
  bool anyWifiProfile = false;
  while (wifiDir.next()) {
    if (wifiDir.isDirectory()) continue;
    String wifiPath = dirChildPath(WIFI_PROFILE_DIR, wifiDir.fileName());
    String listName = basenameOf(wifiPath);
    if (!listName.endsWith(".txt")) continue;
    listName.remove(listName.length() - 4);
    String listText = readWholeFile(wifiPath);
    String listSsid = keyValueFromText(listText, "ssid", "");
    String listChannel = keyValueFromText(listText, "channel", "0");
    String editHref = keyArg.length() ? "/profiles?key=" + urlEscape(keyArg) + "&editwifi=" : "/profiles?editwifi=";
    editHref += urlEscape(listName);
    anyWifiProfile = true;
    html += F("<div class='row'><span><a href='");
    html += editHref;
    html += F("'><strong>");
    html += htmlEscape(listName);
    html += F("</strong></a> <span class='pill'>");
    html += htmlEscape(listSsid.length() ? listSsid : String("(no SSID)"));
    html += F("</span> <span class='pill'>ch ");
    html += htmlEscape(listChannel);
    html += F("</span>");
    if (configGetValue("wifi.profile", "") == listName) html += F(" <span class='pill on'>active</span>");
    html += F("</span><span><a class='btn secondary' href='");
    html += editHref;
    html += F("'>Edit</a></span></div>");
  }
  if (!anyWifiProfile) html += F("<p class='muted'>(empty)</p>");
  html += F("</div><form method='POST' action='/profiles'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='wifi.name' placeholder='wifi profile name'><button name='action' value='wifi_edit'>Load editor</button><button name='action' value='wifi_save'>Save current Wi-Fi</button><button name='action' value='wifi_use'>Use</button><button name='action' value='wifi_reconnect'>Use + reconnect</button> <label><input type='checkbox' name='wifi.confirm' value='yes'> Confirm remove</label> <button name='action' value='wifi_rm'>Remove</button></form></section>");
  html += F("<section class='card'><h2>Create / Edit Wi-Fi Profile</h2><p class='muted'>Leave password blank while editing to keep the stored password.</p><form method='POST' action='/profiles'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='wifi.name' placeholder='profile name' value='");
  html += htmlEscape(editWifiName);
  html += F("'><input name='wifi.ssid' placeholder='SSID' value='");
  html += htmlEscape(editSsid);
  html += F("'><input name='wifi.password' type='password' placeholder='password'><input name='wifi.channel' placeholder='channel 0=auto' value='");
  html += htmlEscape(editChannel);
  html += F("'><input name='wifi.phy' placeholder='phy 11g/11n/11b' value='");
  html += htmlEscape(editPhy);
  html += F("'><input name='wifi.power.dbm' placeholder='power dBm' value='");
  html += htmlEscape(editPower);
  html += F("'><select name='wifi.dhcp'><option value='on'");
  if (editDhcp != "off") html += F(" selected");
  html += F(">DHCP on</option><option value='off'");
  if (editDhcp == "off") html += F(" selected");
  html += F(">static IP</option></select><input name='wifi.ip' placeholder='IP' value='");
  html += htmlEscape(editIp);
  html += F("'><input name='wifi.gateway' placeholder='gateway' value='");
  html += htmlEscape(editGateway);
  html += F("'><input name='wifi.mask' placeholder='mask' value='");
  html += htmlEscape(editMask);
  html += F("'><input name='wifi.dns1' placeholder='dns1' value='");
  html += htmlEscape(editDns1);
  html += F("'><input name='wifi.dns2' placeholder='dns2' value='");
  html += htmlEscape(editDns2);
  html += F("'><button name='action' value='wifi_write'>Save profile</button><button name='action' value='wifi_write_use'>Save + use now</button></form></section>");
  html += F("<section class='card'><h2>Backup / Restore</h2><p><a class='btn' href='/backup");
  html += authQuery();
  html += F("'>Download backup</a><a class='btn secondary' href='/restore");
  html += authQuery();
  html += F("'>Restore backup</a></p><pre>");
  html += htmlEscape(captureOutputForLine("df"));
  html += F("</pre></section>");
  if (result.length()) {
    html += F("<section class='card'><h2>Result</h2><pre>");
    html += htmlEscape(result);
    html += F("</pre></section>");
  }
  html += F("</div>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void handleWebWizard() {
  if (!webAuthOk()) return;
  String keyArg = webServer.arg("key");
  String result;
  if (webServer.method() == HTTP_POST) {
    String kind = webServer.arg("kind");
    String cmd;
    if (kind == "relay") {
      cmd = "relay add " + webServer.arg("name") + " " + webServer.arg("pin") + " " + webServer.arg("mode");
    } else if (kind == "schedule") {
      cmd = "schedule " + webServer.arg("relay") + " " + webServer.arg("on") + " " + webServer.arg("off");
    } else if (kind == "climate") {
      cmd = "climate " + webServer.arg("metric") + " " + webServer.arg("relay") + " " + webServer.arg("low") + " " + webServer.arg("high");
    } else if (kind == "input") {
      cmd = "input add " + webServer.arg("name") + " " + webServer.arg("pin") + " " + webServer.arg("mode");
    }
    if (cmd.length()) result = "$ " + cmd + "\n" + captureOutputForLine(cmd);
  }
  String html = webHeader("KernelESP Wizard", keyArg);
  if (result.length()) {
    html += F("<section class='card'><h2>Result</h2><pre>");
    html += htmlEscape(result);
    html += F("</pre></section>");
  }
  html += F("<div class='grid'><section class='card'><h2>Relay</h2><form method='POST' action='/wizard'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='kind' type='hidden' value='relay'><p><input name='name' placeholder='name' value='light'><input name='pin' placeholder='D1'><select name='mode'><option value='active_low'>active_low</option><option value='active_high'>active_high</option></select></p><p><button>Add relay</button></p></form></section>");
  html += F("<section class='card'><h2>Schedule</h2><form method='POST' action='/wizard'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='kind' type='hidden' value='schedule'><p><input name='relay' placeholder='relay'><input name='on' placeholder='08:00'><input name='off' placeholder='20:00'></p><p><button>Add daily schedule</button></p></form></section></div>");
  html += F("<div class='grid'><section class='card'><h2>Climate Rule</h2><form method='POST' action='/wizard'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='kind' type='hidden' value='climate'><p><select name='metric'><option value='temp'>temperature</option><option value='hum'>humidity</option></select><input name='relay' placeholder='relay'><input name='low' placeholder='low'><input name='high' placeholder='high'></p><p><button>Add rule</button></p></form></section>");
  html += F("<section class='card'><h2>Input</h2><form method='POST' action='/wizard'><input name='key' type='hidden' value='");
  html += htmlEscape(keyArg);
  html += F("'><input name='kind' type='hidden' value='input'><p><input name='name' placeholder='button'><input name='pin' placeholder='D2'><select name='mode'><option value='pullup'>pullup</option><option value='float'>float</option></select></p><p><button>Add input</button></p></form></section></div>");
  html += webFooter();
  webServer.send(200, "text/html", html);
}

void startWeb() {
  if (webRunning) return;
  webServer.collectHeaders("Cookie");
  webServer.on("/style.css", handleWebStyle);
  webServer.on("/login", HTTP_POST, handleWebLogin);
  webServer.on("/", handleWebRoot);
  webServer.on("/ui", handleWebUi);
  webServer.on("/relay", handleWebRelay);
  webServer.on("/relays", handleWebRelaysPage);
  webServer.on("/automations", handleWebAutomations);
  webServer.on("/cmd", handleWebCmd);
  webServer.on("/edit", HTTP_GET, handleWebEdit);
  webServer.on("/save", HTTP_POST, handleWebSave);
  webServer.on("/run", handleWebRun);
  webServer.on("/delete", handleWebDelete);
  webServer.on("/logs", handleWebLogs);
  webServer.on("/settings", handleWebSettings);
  webServer.on("/backup", handleWebBackup);
  webServer.on("/restore", HTTP_ANY, handleWebRestore);
  webServer.on("/diag", handleWebDiag);
  webServer.on("/help", handleWebHelp);
  webServer.on("/profiles", HTTP_ANY, handleWebProfiles);
  webServer.on("/wizard", HTTP_ANY, handleWebWizard);
  webServer.on("/api/status", handleApiStatus);
  webServer.on("/api/sensor", handleApiSensor);
  webServer.on("/api/cmd", handleApiCmd);
  webServer.on("/api/relay", handleApiRelay);
  webServer.onNotFound(handleWebNotFound);
  webServer.begin();
  webRunning = true;
  eventLog("web started");
}

void cmdWeb(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: web start|stop|status")); return; }
  String sub = args[1]; sub.toLowerCase();
  if (sub == "start") {
    startWeb();
    Serial.print(F("web: http://"));
    Serial.println(WiFi.localIP());
  } else if (sub == "stop") {
    webServer.stop();
    webRunning = false;
    eventLog("web stopped");
    Serial.println(F("OK"));
  } else if (sub == "status") {
    Serial.println(webRunning ? F("running") : F("stopped"));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("web: unknown subcommand"));
  }
}

void beginWifiConnect(const String& ssid, const String& password) {
  if (!ssid.length()) return;
  String host = configGetValue("wifi.hostname", "kernelesp");
  WiFi.hostname(host.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.mode(fallbackApRunning ? WIFI_AP_STA : WIFI_STA);
  disconnectWifiStation(false);
  delay(100);
  applyWifiPhyMode();
  applyWifiOutputPower();
  applyWifiIpConfig();
  int channel = wifiConnectChannel();
  if (channel > 0) WiFi.begin(ssid.c_str(), password.c_str(), channel);
  else WiFi.begin(ssid.c_str(), password.c_str());
  wifiConnecting = true;
  wifiWasConnected = false;
  wifiAttemptDisconnectEvents = 0;
  nextWifiWatchMs = 0;
  wifiConnectStartedMs = millis();
  Serial.print(F("wifi connecting: "));
  Serial.println(ssid);
  eventLog("wifi connecting");
}

void connectSavedWifi() {
  if (!fsReady || configGetValue("wifi.autoconnect", "off") != "on") return;
  String ssid = configGetValue("wifi.ssid", "");
  String password = configGetValue("wifi.password", "");
  if (!ssid.length()) return;
  beginWifiConnect(ssid, password);
}

void startFallbackAp() {
  if (fallbackApRunning || configGetValue("fallback.ap", "on") != "on") return;
  String ssid = configGetValue("ap.ssid", "KernelESP-Setup");
  String key = configGetValue("ap.key", "");
  WiFi.mode(WIFI_AP_STA);
  bool ok = key.length() >= 8 ? WiFi.softAP(ssid.c_str(), key.c_str()) : WiFi.softAP(ssid.c_str());
  fallbackApRunning = ok;
  if (ok) {
    eventLog("fallback AP started");
    Serial.print(F("\nap ip "));
    Serial.println(WiFi.softAPIP());
  }
}

void stopFallbackAp() {
  if (!fallbackApRunning) return;
  WiFi.softAPdisconnect(true);
  fallbackApRunning = false;
  if (WiFi.status() != WL_CONNECTED) WiFi.mode(WIFI_STA);
  eventLog("fallback AP stopped");
}

void setupWifiEventHandlers() {
  wifiDisconnectedHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    if ((long)(wifiIgnoreDisconnectEventsUntilMs - millis()) > 0) return;
    lastWifiDisconnectReason = (int)event.reason;
    if (wifiConnecting && wifiAttemptDisconnectEvents < 255) wifiAttemptDisconnectEvents++;
    String msg = "wifi disconnected reason ";
    msg += String(lastWifiDisconnectReason);
    msg += " ";
    msg += wifiDisconnectReasonText(lastWifiDisconnectReason);
    eventLog(msg);
    Serial.print(F("\nwifi disconnected reason: "));
    Serial.print(lastWifiDisconnectReason);
    Serial.print(' ');
    Serial.println(wifiDisconnectReasonText(lastWifiDisconnectReason));
    if (inputLen == 0) printPrompt();
  });
}

void scheduleWifiRetry(const String& reason, bool hardReset) {
  wifiConnecting = false;
  wifiWasConnected = false;
  wifiAttemptDisconnectEvents = 0;
  if (wifiFailCount < 255) wifiFailCount++;
  Serial.print(F("\nwifi retry scheduled: "));
  Serial.print(reason);
  Serial.print(F(" fail_count="));
  Serial.println(wifiFailCount);
  eventLog("wifi retry " + reason);
  if (hardReset) resetWifiRadio();
  unsigned long retryMs = wifiRetryDelayMs();
  nextWifiWatchMs = millis() + retryMs;
  if (wifiFailCount >= WIFI_AP_AFTER_FAILS) startFallbackAp();
  if (wifiFailCount >= 6 && configGetValue("wifi.reboot_on_fail", "off") == "on") {
    eventLog("wifi watchdog reboot");
    delay(100);
    ESP.restart();
  }
  if (inputLen == 0) printPrompt();
}

void processWifi() {
  bool connected = WiFi.status() == WL_CONNECTED;
  if (wifiConnecting && connected) {
    wifiConnecting = false;
    wifiWasConnected = true;
    wifiFailCount = 0;
    wifiAttemptDisconnectEvents = 0;
    ntpRetryCount = 0;
    wifiLastConnectedMs = millis();
    nextWifiWatchMs = millis() + WIFI_WATCHDOG_INTERVAL_MS;
    if (fallbackApRunning) stopFallbackAp();
    Serial.print(F("\nwifi ip "));
    Serial.println(WiFi.localIP());
    eventLog("wifi connected");
    if (configGetValue("ntp.autosync", "on") == "on") {
      ntpPendingSync = true;
      ntpSync(false);
    }
    if (inputLen == 0) printPrompt();
    return;
  }
  if (wifiConnecting &&
      wifiAttemptDisconnectEvents >= WIFI_ATTEMPT_EVENT_FAILS &&
      millis() - wifiConnectStartedMs > 5000UL) {
    scheduleWifiRetry("event failures", true);
    return;
  }
  if (wifiConnecting && millis() - wifiConnectStartedMs > wifiConnectTimeoutMs()) {
    Serial.println(F("\nwifi not connected"));
    eventLog("wifi autoconnect failed");
    scheduleWifiRetry("timeout", true);
    return;
  }
  if (wifiWasConnected && !connected) {
    wifiWasConnected = false;
    eventLog("wifi disconnected");
    Serial.println(F("\nwifi disconnected"));
    if (wifiLastConnectedMs && millis() - wifiLastConnectedMs > 300000UL) wifiFailCount = 0;
    scheduleWifiRetry("disconnect", true);
    return;
  } else if (connected) {
    wifiWasConnected = true;
  }

  unsigned long now = millis();
  if (!connected && !wifiConnecting && (long)(now - nextWifiWatchMs) >= 0) {
    nextWifiWatchMs = now + WIFI_WATCHDOG_INTERVAL_MS;
    if (configGetValue("wifi.watchdog", "on") != "on" ||
        configGetValue("wifi.autoconnect", "off") != "on") return;
    String ssid = configGetValue("wifi.ssid", "");
    String password = configGetValue("wifi.password", "");
    if (ssid.length()) beginWifiConnect(ssid, password);
    if (wifiFailCount >= WIFI_AP_AFTER_FAILS) startFallbackAp();
  }
}

void cmdAp(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "status";
  sub.toLowerCase();
  if (sub == "start") {
    startFallbackAp();
    Serial.println(fallbackApRunning ? F("OK") : F("ap: failed or disabled"));
  } else if (sub == "stop") {
    stopFallbackAp();
    Serial.println(F("OK"));
  } else if (sub == "status") {
    Serial.print(F("running: "));
    Serial.println(fallbackApRunning ? F("yes") : F("no"));
    Serial.print(F("ssid: "));
    Serial.println(configGetValue("ap.ssid", "KernelESP-Setup"));
    Serial.print(F("ip: "));
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println(F("usage: ap start|stop|status"));
  }
}

void cmdBoot(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: boot set|show|run")); return; }
  String sub = args[1]; sub.toLowerCase();
  if (sub == "set") {
    if (argc < 3) { Serial.println(F("usage: boot set <script>")); return; }
    Serial.println(configSetValue("boot.script", normalizePath(args[2])) ? F("OK") : F("boot: set failed"));
  } else if (sub == "show") {
    Serial.println(configGetValue("boot.script", ""));
  } else if (sub == "run") {
    String script = argc >= 3 ? args[2] : configGetValue("boot.script", "");
    if (!script.length()) { Serial.println(F("boot: no script set")); return; }
    runScriptFile(script);
  } else {
    Serial.println(F("boot: unknown subcommand"));
  }
}

void cmdSafe(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "status";
  sub.toLowerCase();
  if (sub == "status") {
    Serial.print(F("boot.safe="));
    Serial.println(configGetValue("boot.safe", "on"));
    Serial.print(F("safe next="));
    Serial.println(LittleFS.exists(SAFE_BOOT_FILE) ? F("yes") : F("no"));
  } else if (sub == "on" || sub == "off") {
    Serial.println(configSetValue("boot.safe", sub) ? F("OK") : F("safe: config failed"));
  } else if (sub == "next") {
    Serial.println(writeWholeFile(SAFE_BOOT_FILE, "1\n") ? F("OK") : F("safe: cannot arm next boot"));
  } else if (sub == "clear") {
    Serial.println(LittleFS.remove(SAFE_BOOT_FILE) ? F("OK") : F("safe: not armed"));
  } else {
    Serial.println(F("usage: safe status|on|off|next|clear"));
  }
}

String uptimeText(bool pretty) {
  unsigned long s = millis() / 1000;
  if (!pretty) {
    return String(s / 86400) + "d " + String((s / 3600) % 24) + "h " +
           String((s / 60) % 60) + "m " + String(s % 60) + "s";
  }
  String out = "up ";
  if (s >= 86400) out += String(s / 86400) + " days, ";
  if (s >= 3600) out += String((s / 3600) % 24) + " hours, ";
  out += String((s / 60) % 60) + " minutes";
  return out;
}

String healthText() {
  FSInfo info;
  if (fsReady) LittleFS.info(info);
  String out;
  out += String(KERNEL_NAME) + " " + KERNEL_VERSION + "\n";
  out += "config_schema: " + configGetValue("system.config_schema", "0") + "\n";
  out += "board: " + configGetValue("board.profile", "generic") + "\n";
  out += "uptime: " + uptimeText(false) + "\n";
  out += "heap: " + String(ESP.getFreeHeap()) + "\n";
  out += "frag: " + String(ESP.getHeapFragmentation()) + "%\n";
  out += "max_block: " + String(ESP.getMaxFreeBlockSize()) + "\n";
  out += "wifi: " + String(WiFi.status() == WL_CONNECTED ? "connected" : (wifiConnecting ? "connecting" : "not connected")) + "\n";
  out += "ip: " + WiFi.localIP().toString() + "\n";
  out += "ap: " + String(fallbackApRunning ? WiFi.softAPIP().toString() : "off") + "\n";
  out += "armed: " + String(automationsArmed ? "on" : "off") + "\n";
  out += "time: " + String(time(nullptr) >= 1600000000 ? "synced" : "not synced") + "\n";
  out += "fs: " + String(fsReady ? "mounted" : "not mounted") + "\n";
  if (fsReady) out += "fs_free: " + String(info.totalBytes - info.usedBytes) + "\n";
  out += "web: " + String(webRunning ? "running" : "stopped") + "\n";
  out += "web.lockout: " + String(webAuthLocked() ? "locked" : configGetValue("web.lockout", "on")) + "\n";
  out += "web.key_default: " + String(webKey() == "admin" ? "yes" : "no") + "\n";
  uint8_t timerCount = 0, ruleCount = 0, cronCount = 0, inputCount = 0;
  for (uint8_t i = 0; i < MAX_TIMERS; i++) if (timers[i].active) timerCount++;
  for (uint8_t i = 0; i < MAX_RULES; i++) if (rules[i].active) ruleCount++;
  for (uint8_t i = 0; i < MAX_CRONS; i++) if (crons[i].active) cronCount++;
  for (uint8_t i = 0; i < MAX_INPUTS; i++) if (inputs[i].active) inputCount++;
  out += "timers: " + String(timerCount) + "\n";
  out += "rules: " + String(ruleCount) + "\n";
  out += "crons: " + String(cronCount) + "\n";
  out += "inputs: " + String(inputCount) + "\n";
  out += "guard.minheap: " + configGetValue("health.minheap", "0") + "\n";
  if (ESP.getFreeHeap() < 12000) out += "warn: low heap\n";
  if (ESP.getHeapFragmentation() > 35) out += "warn: heap fragmentation\n";
  if (webKey() == "admin") out += "warn: default web key\n";
  return out;
}

void cmdHealth(String args[], int argc) {
  if (argc >= 2 && args[1] == "guard") {
    if (argc < 3) {
      Serial.println(configGetValue("health.minheap", "0"));
      return;
    }
    String value = args[2];
    value.toLowerCase();
    if (value == "off") value = "0";
    Serial.println(configSetValue("health.minheap", value) ? F("OK") : F("health: config failed"));
    return;
  }
  Serial.print(healthText());
}

void processHealthGuard() {
  unsigned long now = millis();
  if ((long)(now - nextHealthCheckMs) < 0) return;
  nextHealthCheckMs = now + 60000UL;
  unsigned long minHeap = (unsigned long)configGetValue("health.minheap", "0").toInt();
  if (minHeap > 0 && ESP.getFreeHeap() < minHeap) {
    eventLog("health guard reboot low heap");
    delay(100);
    ESP.restart();
  }
}

void cmdService(String args[], int argc) {
  if (argc < 2) {
    Serial.println(F("web ntp sensor wifi"));
    return;
  }
  String name = args[1]; name.toLowerCase();
  String action = argc >= 3 ? args[2] : "status";
  action.toLowerCase();
  if (name == "web") {
    if (action == "start") startWeb();
    else if (action == "stop") { webServer.stop(); webRunning = false; }
    else if (action == "restart") { webServer.stop(); webRunning = false; startWeb(); }
    else if (action != "status") { Serial.println(F("service: action must be status,start,stop,restart")); return; }
    Serial.println(webRunning ? F("running") : F("stopped"));
  } else if (name == "ntp" || name == "time") {
    if (action == "start" || action == "restart" || action == "kick" || action == "async") ntpKick();
    else if (action == "sync") ntpSync(true);
    else if (action == "stop") configSetValue("ntp.autosync", "off");
    Serial.print(timeStatusText());
  } else if (name == "sensor") {
    if (action == "start" || action == "restart") sensorAutoBegin();
    Serial.print(sensorText());
  } else if (name == "wifi") {
    if (action == "restart") { disconnectWifiStation(false); connectSavedWifi(); }
    else if (action == "stop") { disconnectWifiStation(false); wifiConnecting = false; wifiWasConnected = false; }
    Serial.print(F("status: ")); Serial.println(WiFi.status());
    Serial.print(F("connecting: ")); Serial.println(wifiConnecting ? F("yes") : F("no"));
    Serial.print(F("ip: ")); Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("service: unknown service"));
  }
}

void cmdHostname(String args[], int argc) {
  if (argc < 2) {
    Serial.println(configGetValue("wifi.hostname", "kernelesp"));
    return;
  }
  String host = args[1];
  Serial.println(configSetValue("wifi.hostname", host) ? F("OK") : F("hostname: save failed"));
  WiFi.hostname(host.c_str());
}

int monthIndex(const String& mon) {
  if (mon == "Jan") return 1;
  if (mon == "Feb") return 2;
  if (mon == "Mar") return 3;
  if (mon == "Apr") return 4;
  if (mon == "May") return 5;
  if (mon == "Jun") return 6;
  if (mon == "Jul") return 7;
  if (mon == "Aug") return 8;
  if (mon == "Sep") return 9;
  if (mon == "Oct") return 10;
  if (mon == "Nov") return 11;
  if (mon == "Dec") return 12;
  return 0;
}

long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468L;
}

bool parseHttpDate(const String& headerValue, time_t& epoch) {
  String text = headerValue;
  text.trim();
  int comma = text.indexOf(',');
  if (comma >= 0) text = text.substring(comma + 1);
  text.trim();
  int s1 = text.indexOf(' ');
  int s2 = text.indexOf(' ', s1 + 1);
  int s3 = text.indexOf(' ', s2 + 1);
  int s4 = text.indexOf(' ', s3 + 1);
  if (s1 < 0 || s2 < 0 || s3 < 0) return false;
  int day = text.substring(0, s1).toInt();
  int mon = monthIndex(text.substring(s1 + 1, s2));
  int year = text.substring(s2 + 1, s3).toInt();
  String timePart = s4 > 0 ? text.substring(s3 + 1, s4) : text.substring(s3 + 1);
  int c1 = timePart.indexOf(':');
  int c2 = timePart.indexOf(':', c1 + 1);
  if (day < 1 || day > 31 || mon < 1 || year < 2020 || c1 < 0 || c2 < 0) return false;
  int hour = timePart.substring(0, c1).toInt();
  int minute = timePart.substring(c1 + 1, c2).toInt();
  int second = timePart.substring(c2 + 1).toInt();
  if (hour > 23 || minute > 59 || second > 59) return false;
  epoch = (time_t)(daysFromCivil(year, mon, day) * 86400L + hour * 3600L + minute * 60L + second);
  return epoch >= 1600000000;
}

bool httpTimeSync(const String& host) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient client;
  client.setTimeout(1200);
  if (!client.connect(host.c_str(), 80)) return false;
  client.print(F("HEAD / HTTP/1.0\r\nHost: "));
  client.print(host);
  client.print(F("\r\nConnection: close\r\n\r\n"));
  unsigned long start = millis();
  bool synced = false;
  while ((client.connected() || client.available()) && millis() - start < 5000UL) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (!line.length()) break;
    if (line.startsWith("Date:")) {
      time_t epoch;
      if (parseHttpDate(line.substring(5), epoch)) {
        timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
        synced = true;
        break;
      }
    }
    yield();
  }
  client.stop();
  lastNtpSyncMs = millis();
  if (synced) {
    ntpSyncedOnce = true;
    ntpPendingSync = false;
    ntpRetryCount = 0;
    eventLog("time http synced");
  }
  return synced;
}

bool ntpSync(bool waitForSync) {
  String server1 = configGetValue("ntp.server1", "pool.ntp.org");
  String server2 = configGetValue("ntp.server2", "time.nist.gov");
  long tzSeconds = (long)(configGetValue("ntp.tz", "0").toFloat() * 3600.0f);
  configTime(tzSeconds, 0, server1.c_str(), server2.c_str());
  lastNtpSyncMs = millis();
  if (!waitForSync) return true;
  Serial.print(F("ntp sync"));
  time_t now = time(nullptr);
  for (uint8_t i = 0; i < 40 && now < 1600000000; i++) {
    delay(250);
    Serial.print('.');
    yield();
    now = time(nullptr);
  }
  if (now < 1600000000 && configGetValue("time.http_fallback", "on") == "on") {
    Serial.print(F(" http"));
    if (httpTimeSync(configGetValue("time.http_host", "example.com"))) now = time(nullptr);
  }
  Serial.println();
  ntpSyncedOnce = now >= 1600000000;
  if (ntpSyncedOnce) {
    ntpPendingSync = false;
    ntpRetryCount = 0;
    eventLog("ntp synced");
  }
  return ntpSyncedOnce;
}

bool ntpKick() {
  if (WiFi.status() != WL_CONNECTED) return false;
  ntpPendingSync = true;
  ntpRetryCount = 0;
  ntpSync(false);
  eventLog("ntp kick");
  return true;
}

void processNtp() {
  if (configGetValue("ntp.autosync", "on") != "on" || WiFi.status() != WL_CONNECTED) return;
  time_t now = time(nullptr);
  if (now >= 1600000000) {
    if (!ntpSyncedOnce) {
      ntpSyncedOnce = true;
      ntpPendingSync = false;
      ntpRetryCount = 0;
      eventLog("ntp synced");
    }
    return;
  }
  unsigned long elapsed = millis() - lastNtpSyncMs;
  unsigned long retryMs = NTP_RETRY_MIN_MS;
  for (uint8_t i = 1; i < ntpRetryCount && retryMs < NTP_RETRY_MAX_MS; i++) {
    retryMs *= 2;
  }
  if (retryMs > NTP_RETRY_MAX_MS) retryMs = NTP_RETRY_MAX_MS;
  if ((ntpPendingSync && elapsed > retryMs) || (!ntpPendingSync && elapsed > 3600000UL)) {
    ntpPendingSync = true;
    if (ntpRetryCount < 255) ntpRetryCount++;
    ntpSync(false);
    if (ntpRetryCount >= NTP_HTTP_FALLBACK_RETRIES && configGetValue("time.http_fallback", "on") == "on") {
      httpTimeSync(configGetValue("time.http_host", "example.com"));
    }
  }
}

String timeStatusText() {
  time_t now = time(nullptr);
  String out;
  out += "ntp.autosync=" + configGetValue("ntp.autosync", "on") + "\n";
  out += "ntp.server1=" + configGetValue("ntp.server1", "pool.ntp.org") + "\n";
  out += "ntp.server2=" + configGetValue("ntp.server2", "time.nist.gov") + "\n";
  out += "ntp.tz=" + configGetValue("ntp.tz", "0") + "\n";
  out += "time.http_fallback=" + configGetValue("time.http_fallback", "on") + "\n";
  out += "time.http_host=" + configGetValue("time.http_host", "example.com") + "\n";
  out += "ntp.pending=" + String(ntpPendingSync ? "yes" : "no") + "\n";
  out += "ntp.retry_count=" + String(ntpRetryCount) + "\n";
  if (lastNtpSyncMs) {
    unsigned long elapsed = millis() - lastNtpSyncMs;
    unsigned long retryMs = NTP_RETRY_MIN_MS;
    for (uint8_t i = 1; i < ntpRetryCount && retryMs < NTP_RETRY_MAX_MS; i++) retryMs *= 2;
    if (retryMs > NTP_RETRY_MAX_MS) retryMs = NTP_RETRY_MAX_MS;
    out += "ntp.last_attempt_ms_ago=" + String(elapsed) + "\n";
    out += "ntp.next_retry_ms=" + String(ntpPendingSync && elapsed < retryMs ? retryMs - elapsed : 0) + "\n";
  }
  if (now < 1600000000) {
    out += "time: " + String(ntpPendingSync ? "sync pending" : "not synced") + "\n";
  } else {
    out += "epoch: " + String((unsigned long)now) + "\n";
    out += String(ctime(&now));
  }
  return out;
}

bool parseDateTime(String datePart, String timePart, time_t& epoch) {
  if (datePart.length() != 10 || timePart.length() < 5) return false;
  int y = datePart.substring(0, 4).toInt();
  int mo = datePart.substring(5, 7).toInt();
  int d = datePart.substring(8, 10).toInt();
  int h = timePart.substring(0, 2).toInt();
  int mi = timePart.substring(3, 5).toInt();
  int se = timePart.length() >= 8 ? timePart.substring(6, 8).toInt() : 0;
  if (y < 2020 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 || se > 59) return false;
  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = y - 1900;
  tmv.tm_mon = mo - 1;
  tmv.tm_mday = d;
  tmv.tm_hour = h;
  tmv.tm_min = mi;
  tmv.tm_sec = se;
  epoch = mktime(&tmv);
  return epoch > 1600000000;
}

void cmdDate(String args[], int argc) {
  if (argc >= 2 && args[1] == "set") {
    if (argc < 4) { Serial.println(F("usage: date set YYYY-MM-DD HH:MM[:SS]")); return; }
    time_t epoch;
    if (!parseDateTime(args[2], args[3], epoch)) { Serial.println(F("date: bad date/time")); return; }
    timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
    ntpSyncedOnce = true;
    Serial.print(dateText());
    return;
  }
  if (argc >= 2 && args[1] == "-u") {
    time_t now = time(nullptr);
    if (now < 1600000000) { Serial.println(F("date: time not synced")); return; }
    struct tm* tm = gmtime(&now);
    char buf[80];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d UTC",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    Serial.println(buf);
    return;
  }
  Serial.print(dateText());
}

void cmdTimeNet(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "status";
  sub.toLowerCase();
  if (sub == "kick" || sub == "async") {
    Serial.println(ntpKick() ? F("ntp kick queued") : F("ntp: wifi not connected"));
  } else if (sub == "sync" || sub == "ntp") {
    if (!ntpSync(true)) Serial.println(F("time: not synced"));
    else Serial.print(timeStatusText());
  } else if (sub == "http") {
    String host = argc >= 3 ? args[2] : configGetValue("time.http_host", "example.com");
    if (!httpTimeSync(host)) Serial.println(F("time: http sync failed"));
    else Serial.print(timeStatusText());
  } else if (sub == "status" || sub == "show") {
    Serial.print(timeStatusText());
  } else if (sub == "auto") {
    if (argc < 3) { Serial.println(configGetValue("ntp.autosync", "on")); return; }
    String value = args[2]; value.toLowerCase();
    if (value != "on" && value != "off") { Serial.println(F("usage: ntp auto on|off")); return; }
    Serial.println(configSetValue("ntp.autosync", value) ? F("OK") : F("ntp: config failed"));
  } else if (sub == "server") {
    if (argc < 3) {
      Serial.println(configGetValue("ntp.server1", "pool.ntp.org"));
      Serial.println(configGetValue("ntp.server2", "time.nist.gov"));
      return;
    }
    configSetValue("ntp.server1", args[2]);
    if (argc >= 4) configSetValue("ntp.server2", args[3]);
    Serial.println(F("OK"));
  } else if (sub == "tz") {
    if (argc < 3) { Serial.println(configGetValue("ntp.tz", "0")); return; }
    Serial.println(configSetValue("ntp.tz", args[2]) ? F("OK") : F("ntp: tz failed"));
  } else if (sub == "fallback") {
    if (argc < 3) {
      Serial.println(configGetValue("time.http_fallback", "on"));
      return;
    }
    String value = args[2]; value.toLowerCase();
    if (value != "on" && value != "off") { Serial.println(F("usage: ntp fallback on|off")); return; }
    Serial.println(configSetValue("time.http_fallback", value) ? F("OK") : F("ntp: fallback failed"));
  } else if (sub == "httphost") {
    if (argc < 3) {
      Serial.println(configGetValue("time.http_host", "example.com"));
      return;
    }
    Serial.println(configSetValue("time.http_host", args[2]) ? F("OK") : F("ntp: httphost failed"));
  } else {
    Serial.println(F("usage: ntp kick|sync|http|status|auto|server|tz|fallback|httphost"));
  }
}

void cmdPing(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: ping <host> [port]")); return; }
  String host = args[1];
  uint16_t port = argc >= 3 ? args[2].toInt() : 80;
  if (port == 0) port = 80;
  WiFiClient client;
  unsigned long start = millis();
  bool ok = client.connect(host.c_str(), port);
  unsigned long elapsed = millis() - start;
  client.stop();
  if (ok) {
    Serial.print(F("tcp "));
    Serial.print(host);
    Serial.print(':');
    Serial.print(port);
    Serial.print(F(" "));
    Serial.print(elapsed);
    Serial.println(F("ms"));
  } else {
    Serial.println(F("ping: connect failed"));
  }
}

void cmdHttpGet(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: httpget http://host[:port]/path")); return; }
  String url = args[1];
  if (!url.startsWith("http://")) { Serial.println(F("httpget: only http:// is supported")); return; }
  String rest = url.substring(7);
  int slash = rest.indexOf('/');
  String hostPort = slash >= 0 ? rest.substring(0, slash) : rest;
  String path = slash >= 0 ? rest.substring(slash) : "/";
  uint16_t port = 80;
  int colon = hostPort.indexOf(':');
  String host = hostPort;
  if (colon > 0) {
    host = hostPort.substring(0, colon);
    port = hostPort.substring(colon + 1).toInt();
    if (port == 0) { Serial.println(F("httpget: bad port")); return; }
  }
  WiFiClient client;
  if (!client.connect(host.c_str(), port)) { Serial.println(F("httpget: connect failed")); return; }
  client.print(F("GET "));
  client.print(path);
  client.print(F(" HTTP/1.0\r\nHost: "));
  client.print(host);
  client.print(F("\r\nConnection: close\r\n\r\n"));
  unsigned long start = millis();
  uint16_t count = 0;
  while ((client.connected() || client.available()) && millis() - start < 8000) {
    while (client.available()) {
      char c = client.read();
      if (count++ < 1500) Serial.write(c);
      yield();
    }
    yield();
  }
  client.stop();
  if (count >= 1500) Serial.println(F("\n[truncated]"));
}

String mailHeaderValue(String value) {
  value.replace("\r", " ");
  value.replace("\n", " ");
  value.trim();
  if (value.length() > 120) value = value.substring(0, 120);
  return value;
}

String mailAddressValue(String value) {
  value = mailHeaderValue(value);
  value.replace("<", "");
  value.replace(">", "");
  return value;
}

bool smtpReadCode(WiFiClient& client, uint16_t& code) {
  unsigned long deadline = millis() + 8000UL;
  code = 0;
  while ((long)(deadline - millis()) > 0) {
    if (!client.connected() && !client.available()) return false;
    if (!client.available()) {
      delay(10);
      yield();
      continue;
    }
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() < 3) continue;
    uint16_t parsed = (uint16_t)line.substring(0, 3).toInt();
    if (parsed > 0) code = parsed;
    if (line.length() < 4 || line[3] != '-') return code > 0;
  }
  return false;
}

bool smtpExpect(WiFiClient& client, uint16_t minCode, uint16_t maxCode, const __FlashStringHelper* stage) {
  uint16_t code = 0;
  if (!smtpReadCode(client, code) || code < minCode || code > maxCode) {
    Serial.print(F("mail: SMTP "));
    Serial.print(stage);
    Serial.print(F(" failed"));
    if (code) {
      Serial.print(F(" code="));
      Serial.print(code);
    }
    Serial.println();
    return false;
  }
  return true;
}

bool smtpCommand(WiFiClient& client, const String& command, uint16_t minCode, uint16_t maxCode, const __FlashStringHelper* stage) {
  client.print(command);
  client.print(F("\r\n"));
  return smtpExpect(client, minCode, maxCode, stage);
}

void smtpPrintBody(WiFiClient& client, String body) {
  body.replace("\r", "");
  int start = 0;
  bool wrote = false;
  while (start <= (int)body.length()) {
    int end = body.indexOf('\n', start);
    String line = end < 0 ? body.substring(start) : body.substring(start, end);
    if (line.startsWith(".")) client.print('.');
    client.print(line);
    client.print(F("\r\n"));
    wrote = true;
    if (end < 0) break;
    start = end + 1;
    yield();
  }
  if (!wrote) client.print(F("(empty)\r\n"));
}

bool sendMailMessage(const String& toArg, const String& subjectArg, const String& bodyArg) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("mail: Wi-Fi not connected"));
    return false;
  }
  String host = configGetValue("mail.smtp.host", "");
  host.trim();
  if (!host.length()) {
    Serial.println(F("mail: set mail.smtp.host first"));
    return false;
  }
  uint16_t port = (uint16_t)configGetValue("mail.smtp.port", "25").toInt();
  if (port == 0) port = 25;
  String from = mailAddressValue(configGetValue("mail.from", "kernelesp@local"));
  String to = toArg == "default" || toArg == "-" ? configGetValue("mail.to", "") : toArg;
  to = mailAddressValue(to);
  String subject = mailHeaderValue(subjectArg);
  String helo = mailHeaderValue(configGetValue("mail.helo", configGetValue("wifi.hostname", "kernelesp")));
  if (!from.length() || !to.length() || !subject.length()) {
    Serial.println(F("mail: from, to and subject are required"));
    return false;
  }

  WiFiClient client;
  client.setTimeout(2500);
  if (!client.connect(host.c_str(), port)) {
    Serial.println(F("mail: connect failed"));
    eventLog("mail connect failed");
    return false;
  }
  bool ok = smtpExpect(client, 200, 299, F("banner"));
  if (ok) {
    client.print(F("EHLO "));
    client.print(helo);
    client.print(F("\r\n"));
    uint16_t code = 0;
    ok = smtpReadCode(client, code) && code >= 200 && code <= 299;
    if (!ok) ok = smtpCommand(client, "HELO " + helo, 200, 299, F("helo"));
  }
  if (ok) ok = smtpCommand(client, "MAIL FROM:<" + from + ">", 200, 299, F("mail from"));
  if (ok) ok = smtpCommand(client, "RCPT TO:<" + to + ">", 200, 299, F("rcpt to"));
  if (ok) ok = smtpCommand(client, "DATA", 300, 399, F("data"));
  if (ok) {
    client.print(F("From: "));
    client.print(from);
    client.print(F("\r\nTo: "));
    client.print(to);
    client.print(F("\r\nSubject: "));
    client.print(subject);
    client.print(F("\r\nContent-Type: text/plain; charset=utf-8\r\nX-Mailer: KernelESP "));
    client.print(F(KERNEL_VERSION));
    client.print(F("\r\n\r\n"));
    smtpPrintBody(client, bodyArg);
    client.print(F(".\r\n"));
    ok = smtpExpect(client, 200, 299, F("message"));
  }
  client.print(F("QUIT\r\n"));
  client.stop();
  eventLog(ok ? "mail sent" : "mail failed");
  Serial.println(ok ? F("mail: sent") : F("mail: failed"));
  return ok;
}

void cmdMail(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "status";
  sub.toLowerCase();
  if (sub == "status") {
    Serial.println(F("mail.smtp.mode=plain"));
    Serial.println(F("mail.smtp.tls=off"));
    Serial.println(F("mail.smtp.auth=off"));
    Serial.println("mail.smtp.host=" + configGetValue("mail.smtp.host", ""));
    Serial.println("mail.smtp.port=" + configGetValue("mail.smtp.port", "25"));
    Serial.println("mail.from=" + configGetValue("mail.from", "kernelesp@local"));
    Serial.println("mail.to=" + configGetValue("mail.to", ""));
    Serial.println("mail.helo=" + configGetValue("mail.helo", configGetValue("wifi.hostname", "kernelesp")));
  } else if (sub == "config") {
    if (argc < 3) {
      Serial.println(F("usage: mail config <smtp-host> [port] [from] [default-to]"));
      return;
    }
    bool ok = configSetValue("mail.smtp.host", args[2]);
    if (argc >= 4) ok = ok && configSetValue("mail.smtp.port", args[3]);
    if (argc >= 5) ok = ok && configSetValue("mail.from", args[4]);
    if (argc >= 6) ok = ok && configSetValue("mail.to", args[5]);
    Serial.println(ok ? F("OK") : F("mail: config failed"));
  } else if (sub == "host" || sub == "port" || sub == "from" || sub == "to" || sub == "helo") {
    String key = sub == "host" ? "mail.smtp.host" : (sub == "port" ? "mail.smtp.port" : "mail." + sub);
    if (argc < 3) {
      Serial.println(configGetValue(key, sub == "port" ? "25" : ""));
      return;
    }
    Serial.println(configSetValue(key, args[2]) ? F("OK") : F("mail: save failed"));
  } else if (sub == "send") {
    if (argc < 5) {
      Serial.println(F("usage: mail send <to|default|-> <subject> <message>"));
      return;
    }
    sendMailMessage(args[2], args[3], joinArgs(args, argc, 4));
  } else if (sub == "test") {
    String message = argc >= 3 ? joinArgs(args, argc, 2) : "KernelESP test message";
    sendMailMessage("default", "KernelESP test", message);
  } else if (sub == "health") {
    String subject = argc >= 3 ? joinArgs(args, argc, 2) : "KernelESP health";
    String body = "KernelESP health report\n\n";
    body += "Date:\n";
    body += dateText();
    body += "\nSystem:\n";
    body += healthText();
    body += "\nSensor:\n";
    body += sensorText();
    sendMailMessage("default", subject, body);
  } else {
    Serial.println(F("usage: mail status|config|host|port|from|to|helo|send|test|health"));
  }
}

void cmdPathTool(const String& cmd, String args[], int argc) {
  if (argc < 2) {
    Serial.println(cmd == "basename" ? F("usage: basename <path>") : F("usage: dirname <path>"));
    return;
  }
  String path = normalizePath(args[1]);
  Serial.println(cmd == "basename" ? basenameOf(path) : parentPath(path));
}

void cmdTest(String args[], int argc) {
  bool ok = false;
  uint8_t end = argc;
  if (argc > 1 && args[argc - 1] == "]") end--;
  if (end >= 3 && args[1] == "-f") ok = LittleFS.exists(normalizePath(args[2])) && !isDirectory(normalizePath(args[2]));
  else if (end >= 3 && args[1] == "-d") ok = isDirectory(normalizePath(args[2]));
  else if (end >= 3 && args[1] == "-e") ok = pathExists(normalizePath(args[2]));
  else if (end >= 3 && args[1] == "-n") ok = args[2].length() > 0;
  else if (end >= 3 && args[1] == "-z") ok = args[2].length() == 0;
  else if (end >= 4 && args[2] == "=") ok = args[1] == args[3];
  else if (end >= 4 && args[2] == "!=") ok = args[1] != args[3];
  else if (end >= 2) ok = args[1].length() > 0;
  else { Serial.println(F("usage: test -f|-d|-e <path> | test a = b")); return; }
  Serial.println(ok ? F("true") : F("false"));
}

uint8_t condOpFromToken(String token) {
  token.toLowerCase();
  if (token == "=" || token == "==" || token == "eq" || token == "is") return 1;
  if (token == "!=" || token == "=!" || token == "<>" || token == "ne" || token == "not") return 2;
  if (token == "<" || token == "lt" || token == "before") return 3;
  if (token == ">" || token == "gt" || token == "after") return 4;
  if (token == "<=" || token == "=<" || token == "le") return 5;
  if (token == ">=" || token == "=>" || token == "ge") return 6;
  return 0;
}

bool compareFloat(float left, uint8_t op, float right) {
  if (op == 1) return fabs(left - right) < 0.001f;
  if (op == 2) return fabs(left - right) >= 0.001f;
  if (op == 3) return left < right;
  if (op == 4) return left > right;
  if (op == 5) return left <= right;
  if (op == 6) return left >= right;
  return false;
}

bool compareStringValue(String left, uint8_t op, String right) {
  if (op == 1) return left == right;
  if (op == 2) return left != right;
  return false;
}

bool currentMinuteOfDay(int& minuteOfDay) {
  time_t now = time(nullptr);
  if (now < 1600000000) return false;
  struct tm* tm = localtime(&now);
  if (!tm) return false;
  minuteOfDay = tm->tm_hour * 60 + tm->tm_min;
  return true;
}

bool isNumericLiteral(String token) {
  token.trim();
  if (!token.length()) return false;
  if (token[0] == '-') token = token.substring(1);
  if (!token.length()) return false;
  bool digit = false;
  bool dot = false;
  for (uint16_t i = 0; i < token.length(); i++) {
    char c = token[i];
    if (c == '.' && !dot) {
      dot = true;
    } else if (isDigit(c)) {
      digit = true;
    } else {
      return false;
    }
  }
  return digit;
}

bool storedConditionValue(String token, float& value, bool& numeric, String& text) {
  String key = token;
  key.trim();
  if (key.startsWith("$")) key = key.substring(1);
  String lowered = key;
  lowered.toLowerCase();
  String stored;
  if (!kvGetMaybe(CONF_DEFINES, lowered, stored) && !kvGetMaybe(CONF_STATE, lowered, stored)) return false;
  if (isNumericLiteral(stored)) {
    numeric = true;
    value = stored.toFloat();
  } else {
    numeric = false;
    text = stored;
    text.toLowerCase();
  }
  return true;
}

bool conditionValue(String token, float& value, bool& numeric, String& text) {
  token.trim();
  String original = token;
  token.toLowerCase();
  numeric = true;
  uint8_t metric = ruleMetricFromToken(token);
  if (metric) {
    SensorReading reading = readBme();
    return ruleMetricValue(reading, metric, value);
  }
  if (token == "time" || token == "clock") {
    int minutes;
    if (!currentMinuteOfDay(minutes)) return false;
    value = minutes;
    return true;
  }
  if (token == "armed") {
    numeric = false;
    text = automationsArmed ? "on" : "off";
    return true;
  }
  if (token == "wifi") {
    numeric = false;
    text = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
    return true;
  }
  if (isNumericLiteral(original)) {
    value = original.toFloat();
    return true;
  }
  if (storedConditionValue(original, value, numeric, text)) return true;
  numeric = false;
  text = token;
  return true;
}

bool conditionRightValue(const String& lhs, String token, float& value, bool& numeric, String& text) {
  String left = lhs;
  left.toLowerCase();
  if (left == "time" || left == "clock") {
    uint8_t h, m;
    if (!parseClockTime(token, h, m)) return false;
    numeric = true;
    value = h * 60 + m;
    return true;
  }
  return conditionValue(token, value, numeric, text);
}

bool evalCondition(String lhs, String opText, String rhs) {
  uint8_t op = condOpFromToken(opText);
  if (!op) return false;
  float leftValue = 0, rightValue = 0;
  bool leftNumeric = true, rightNumeric = true;
  String leftText, rightText;
  if (!conditionValue(lhs, leftValue, leftNumeric, leftText)) return false;
  if (!conditionRightValue(lhs, rhs, rightValue, rightNumeric, rightText)) return false;
  if (leftNumeric && rightNumeric) return compareFloat(leftValue, op, rightValue);
  return compareStringValue(leftText, op, rightText);
}

bool isWordChar(char c) {
  return isAlphaNumeric(c) || c == '_';
}

String lowerCopy(String value) {
  value.toLowerCase();
  return value;
}

String unquoteValue(String token) {
  token.trim();
  if (token.length() >= 2) {
    char first = token[0];
    char last = token[token.length() - 1];
    if ((first == '"' || first == '\'') && first == last) return token.substring(1, token.length() - 1);
  }
  return token;
}

bool truthyConditionValue(String token) {
  float value = 0;
  bool numeric = true;
  String text;
  if (!conditionValue(unquoteValue(token), value, numeric, text)) return false;
  if (numeric) return fabs(value) >= 0.001f;
  text.toLowerCase();
  return text == "on" || text == "true" || text == "yes" || text == "connected" || text == "high" || text == "1";
}

class ConditionParser {
public:
  ConditionParser(const String& expression) : text(expression), pos(0), failed(false) {}

  bool parse(bool& result) {
    result = parseOr();
    skipSpaces();
    if (pos < (int)text.length()) failed = true;
    return !failed;
  }

private:
  String text;
  int pos;
  bool failed;

  void skipSpaces() {
    while (pos < (int)text.length() && isSpace(text[pos])) pos++;
  }

  bool matchChar(char expected) {
    skipSpaces();
    if (pos < (int)text.length() && text[pos] == expected) {
      pos++;
      return true;
    }
    return false;
  }

  bool matchOp(const char* op) {
    skipSpaces();
    int len = strlen(op);
    if (text.substring(pos, pos + len) == op) {
      pos += len;
      return true;
    }
    return false;
  }

  bool matchWord(const char* word) {
    skipSpaces();
    int len = strlen(word);
    if (pos + len > (int)text.length()) return false;
    String part = text.substring(pos, pos + len);
    part.toLowerCase();
    if (part != word) return false;
    if (pos > 0 && isWordChar(text[pos - 1])) return false;
    if (pos + len < (int)text.length() && isWordChar(text[pos + len])) return false;
    pos += len;
    return true;
  }

  String readQuoted() {
    char quote = text[pos++];
    String out;
    while (pos < (int)text.length()) {
      char c = text[pos++];
      if (c == quote) break;
      out += c;
    }
    return out;
  }

  String readOperand() {
    skipSpaces();
    if (pos >= (int)text.length()) return "";
    if (text[pos] == '"' || text[pos] == '\'') return readQuoted();
    String out;
    while (pos < (int)text.length()) {
      char c = text[pos];
      if (isSpace(c) || c == '(' || c == ')' || c == '&' || c == '|' || c == '<' || c == '>' || c == '=' || c == '!') break;
      out += c;
      pos++;
    }
    out.trim();
    return out;
  }

  String readComparisonOp() {
    skipSpaces();
    if (pos >= (int)text.length()) return "";
    const char* ops[] = {"<=", ">=", "==", "!=", "=!", "<>", "<", ">", "="};
    for (uint8_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
      int len = strlen(ops[i]);
      if (text.substring(pos, pos + len) == ops[i]) {
        pos += len;
        return String(ops[i]);
      }
    }
    int start = pos;
    String word = readOperand();
    String lw = lowerCopy(word);
    if (condOpFromToken(lw)) return word;
    pos = start;
    return "";
  }

  bool parseComparison() {
    String left = readOperand();
    if (!left.length()) {
      failed = true;
      return false;
    }
    String op = readComparisonOp();
    if (!op.length()) return truthyConditionValue(left);
    String right = readOperand();
    if (!right.length()) {
      failed = true;
      return false;
    }
    return evalCondition(unquoteValue(left), op, unquoteValue(right));
  }

  bool parseUnary() {
    skipSpaces();
    if (matchOp("!")) return !parseUnary();
    if (matchWord("not")) return !parseUnary();
    if (matchChar('(')) {
      bool value = parseOr();
      if (!matchChar(')')) failed = true;
      return value;
    }
    return parseComparison();
  }

  bool parseAnd() {
    bool value = parseUnary();
    while (!failed) {
      if (matchOp("&&") || matchWord("and")) value = parseUnary() && value;
      else break;
      yield();
    }
    return value;
  }

  bool parseOr() {
    bool value = parseAnd();
    while (!failed) {
      if (matchOp("||") || matchWord("or")) value = parseAnd() || value;
      else break;
      yield();
    }
    return value;
  }
};

bool findIfCommandSeparator(const String& expr, int& sepStart, int& sepEnd) {
  bool quoted = false;
  char quoteChar = '\0';
  int depth = 0;
  for (int i = 0; i < (int)expr.length(); i++) {
    char c = expr[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
      continue;
    }
    if (quoted) continue;
    if (c == '(') depth++;
    else if (c == ')' && depth > 0) depth--;
    if (depth != 0) continue;
    if ((i == 0 || !isWordChar(expr[i - 1])) && (expr.substring(i, i + 4) == "then") && (i + 4 >= (int)expr.length() || !isWordChar(expr[i + 4]))) {
      sepStart = i;
      sepEnd = i + 4;
      return true;
    }
    if ((i == 0 || !isWordChar(expr[i - 1])) && (expr.substring(i, i + 2) == "do") && (i + 2 >= (int)expr.length() || !isWordChar(expr[i + 2]))) {
      sepStart = i;
      sepEnd = i + 2;
      return true;
    }
  }
  return false;
}

bool extractParenIfExpression(String expr, String& condition, String& command) {
  expr.trim();
  if (!expr.startsWith("(")) return false;
  bool quoted = false;
  char quoteChar = '\0';
  int depth = 0;
  for (int i = 0; i < (int)expr.length(); i++) {
    char c = expr[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
      continue;
    }
    if (quoted) continue;
    if (c == '(') depth++;
    else if (c == ')') {
      depth--;
      if (depth == 0) {
        condition = expr.substring(1, i);
        command = expr.substring(i + 1);
        command.trim();
        String cmdLower = lowerCopy(command);
        if (cmdLower.startsWith("then ")) command = command.substring(5);
        else if (cmdLower == "then") command = "";
        else if (cmdLower.startsWith("do ")) command = command.substring(3);
        else if (cmdLower == "do") command = "";
        command.trim();
        return true;
      }
    }
  }
  return false;
}

bool looksLikeCCondition(const String& expr) {
  return expr.indexOf("&&") >= 0 || expr.indexOf("||") >= 0 || expr.indexOf('!') >= 0 || expr.indexOf('(') >= 0;
}

bool evalCCondition(String condition, bool quiet, bool& ok) {
  condition.trim();
  ConditionParser parser(condition);
  if (!parser.parse(ok)) {
    if (!quiet) Serial.println(F("if: bad C-like condition"));
    return false;
  }
  return true;
}

bool findElseSeparator(const String& command, int& sepStart, int& sepEnd) {
  bool quoted = false;
  char quoteChar = '\0';
  int parenDepth = 0;
  int braceDepth = 0;
  for (int i = 0; i < (int)command.length(); i++) {
    char c = command[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
      continue;
    }
    if (quoted) continue;
    if (c == '(') parenDepth++;
    else if (c == ')' && parenDepth > 0) parenDepth--;
    else if (c == '{') braceDepth++;
    else if (c == '}' && braceDepth > 0) braceDepth--;
    if (parenDepth != 0 || braceDepth != 0) continue;
    if ((i == 0 || !isWordChar(command[i - 1])) && (command.substring(i, i + 4) == "else") && (i + 4 >= (int)command.length() || !isWordChar(command[i + 4]))) {
      sepStart = i;
      sepEnd = i + 4;
      return true;
    }
  }
  return false;
}

bool splitElseBlocks(String command, String& trueBlock, String& falseBlock) {
  int sepStart = -1, sepEnd = -1;
  if (!findElseSeparator(command, sepStart, sepEnd)) {
    trueBlock = command;
    falseBlock = "";
    trueBlock.trim();
    return false;
  }
  trueBlock = command.substring(0, sepStart);
  falseBlock = command.substring(sepEnd);
  trueBlock.trim();
  falseBlock.trim();
  return true;
}

String stripOuterBlock(String block) {
  block.trim();
  if (block.startsWith("{") && block.endsWith("}") && block.length() >= 2) {
    block = block.substring(1, block.length() - 1);
    block.trim();
  }
  return block;
}

void runCommandBlock(String block) {
  block = stripOuterBlock(block);
  block.replace("\r", "\n");
  block.replace("\n", ";");
  String commands[10];
  uint8_t commandCount = 0;
  String current;
  bool quoted = false;
  char quoteChar = '\0';
  int parenDepth = 0;
  int braceDepth = 0;
  for (uint16_t i = 0; i < block.length(); i++) {
    char c = block[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
      current += c;
    } else if (!quoted && c == '(') {
      parenDepth++;
      current += c;
    } else if (!quoted && c == ')' && parenDepth > 0) {
      parenDepth--;
      current += c;
    } else if (!quoted && c == '{') {
      braceDepth++;
      current += c;
    } else if (!quoted && c == '}' && braceDepth > 0) {
      braceDepth--;
      current += c;
    } else if (!quoted && c == ';' && parenDepth == 0 && braceDepth == 0) {
      current = stripCLineComment(current);
      current.trim();
      if (current.length() && !current.startsWith("#") && !current.startsWith("//")) {
        if (commandCount >= 10) { Serial.println(F("block: too many commands")); return; }
        commands[commandCount++] = current;
      }
      current = "";
      yield();
    } else {
      current += c;
    }
  }
  current = stripCLineComment(current);
  current.trim();
  if (current.length() && !current.startsWith("#") && !current.startsWith("//")) {
    if (commandCount >= 10) { Serial.println(F("block: too many commands")); return; }
    commands[commandCount++] = current;
  }
  for (uint8_t i = 0; i < commandCount; i++) {
    String c = commands[i];
    String lower = c;
    lower.toLowerCase();
    if (lower == "if" || lower.startsWith("if ")) {
      if (ifBranchRunning) Serial.println(F("block: nested if not supported"));
      else runIfExpression(c.substring(2), false);
    } else if (ifBranchRunning && lower.startsWith("call ")) {
      Serial.println(F("block: function calls in if not supported"));
    } else if (ifBranchRunning && functionExists(c)) {
      Serial.println(F("block: function calls in if not supported"));
    }
    else executeLine(c);
    yield();
  }
}

bool runCIfExpression(String expr, bool quiet) {
  String condition, command;
  if (!extractParenIfExpression(expr, condition, command)) {
    int sepStart = -1, sepEnd = -1;
    if (!findIfCommandSeparator(expr, sepStart, sepEnd)) return false;
    condition = expr.substring(0, sepStart);
    command = expr.substring(sepEnd);
    condition.trim();
    command.trim();
  }
  if (!condition.length()) {
    if (!quiet) Serial.println(F("if: missing condition"));
    return true;
  }
  if (!command.length()) {
    if (!quiet) Serial.println(F("if: missing command"));
    return true;
  }
  bool ok = false;
  if (!evalCCondition(condition, quiet, ok)) return true;
  String trueBlock, falseBlock;
  bool hasElse = splitElseBlocks(command, trueBlock, falseBlock);
  bool previousIfBranch = ifBranchRunning;
  ifBranchRunning = true;
  if (ok) runCommandBlock(trueBlock);
  else if (hasElse && falseBlock.length()) runCommandBlock(falseBlock);
  else if (!quiet) Serial.println(F("false"));
  ifBranchRunning = previousIfBranch;
  return true;
}

bool runIfExpression(String expr, bool quiet) {
  expr.trim();
  if (expr.startsWith("(") || looksLikeCCondition(expr)) {
    if (runCIfExpression(expr, quiet)) return true;
  }

  String parts[24];
  int count = splitArgs(expr, parts, 24);
  if (count < 5) {
    if (!quiet) Serial.println(F("if: usage if (temp >= 40 && time < 10:00) relay on fan"));
    return false;
  }
  int pos = 0;
  bool ok = true;
  while (pos < count) {
    String marker = parts[pos];
    marker.toLowerCase();
    if (marker == "then") { pos++; break; }
    if (marker == "if" || marker == "and") pos++;
    if (pos + 2 >= count) {
      if (!quiet) Serial.println(F("if: bad condition"));
      return false;
    }
    ok = ok && evalCondition(parts[pos], parts[pos + 1], parts[pos + 2]);
    pos += 3;
    if (pos < count) {
      String next = parts[pos];
      next.toLowerCase();
      if (next == "then") { pos++; break; }
      if (next != "if" && next != "and" && next != "&&") {
        if (!quiet) Serial.println(F("if: expected if, and, &&, or then"));
        return false;
      }
    }
    yield();
  }
  if (pos >= count) {
    if (!quiet) Serial.println(F("if: missing then command"));
    return false;
  }
  String command = joinArgs(parts, count, pos);
  if (!command.length()) {
    if (!quiet) Serial.println(F("if: missing command"));
    return false;
  }
  if (ok) {
    bool previousIfBranch = ifBranchRunning;
    ifBranchRunning = true;
    runCommandBlock(command);
    ifBranchRunning = previousIfBranch;
  } else if (!quiet) Serial.println(F("false"));
  return ok;
}

void cmdIf(String args[], int argc) {
  if (argc < 2) {
    Serial.println(F("usage: if (temp >= 40 && time < 10:00) relay on fan"));
    return;
  }
  runIfExpression(joinArgs(args, argc, 1), false);
}

String inputEdgeName(String edge) {
  edge.toLowerCase();
  if (edge == "pulse" || edge == "change") return "change";
  if (edge == "high" || edge == "on" || edge == "rising") return "high";
  if (edge == "low" || edge == "off" || edge == "falling") return "low";
  return "";
}

int ensurePinInput(const String& pinToken, const String& modeToken) {
  String name = "pin_" + safeNameToken(pinToken);
  int idx = findInput(name);
  if (idx >= 0) return idx;
  int pin;
  if (!resolvePin(pinToken, pin)) return -1;
  bool pullup = modeToken != "float";
  for (uint8_t i = 0; i < MAX_INPUTS; i++) {
    if (inputs[i].active) continue;
    inputs[i] = InputWatcher();
    inputs[i].name = name;
    inputs[i].pin = pin;
    inputs[i].pullup = pullup;
    inputs[i].debounceMs = 50;
    pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
    inputs[i].stableState = digitalRead(pin);
    inputs[i].lastRead = inputs[i].stableState;
    inputs[i].lastChangeMs = millis();
    inputs[i].active = true;
    return i;
  }
  Serial.println(F("input: table full"));
  return -1;
}

void setInputEdgeCommand(uint8_t idx, const String& edge, const String& command) {
  if (edge == "high") inputs[idx].highCommand = command;
  else if (edge == "low") inputs[idx].lowCommand = command;
  else inputs[idx].changeCommand = command;
}

void cmdWhen(String args[], int argc) {
  String parts[24];
  int count = splitArgs(joinArgs(args, argc, 1), parts, 24);
  if (count < 7) {
    Serial.println(F("usage: when input <name> high|low|pulse if (<expr>) <cmd>"));
    Serial.println(F("   or: when pin <pin> high|low|pulse [pullup|float] if (<expr>) <cmd>"));
    return;
  }
  String target = parts[0];
  target.toLowerCase();
  int idx = -1;
  int pos = 3;
  if (target == "input") {
    idx = findInput(parts[1]);
    if (idx < 0) { Serial.println(F("input: not found")); return; }
  } else if (target == "pin") {
    String mode = "pullup";
    if (count > 3) {
      String maybeMode = parts[3];
      maybeMode.toLowerCase();
      if (maybeMode == "pullup" || maybeMode == "float") {
        mode = maybeMode;
        pos = 4;
      }
    }
    idx = ensurePinInput(parts[1], mode);
    if (idx < 0) return;
  } else {
    Serial.println(F("when: target must be input or pin"));
    return;
  }
  String edge = inputEdgeName(parts[2]);
  if (!edge.length()) { Serial.println(F("when: edge must be high, low, pulse, or change")); return; }
  if (pos >= count || parts[pos] != "if") { Serial.println(F("when: expected if condition")); return; }
  String command = joinArgs(parts, count, pos);
  setInputEdgeCommand(idx, edge, command);
  saveInputs();
  Serial.print(F("when "));
  Serial.print(inputs[idx].name);
  Serial.print(' ');
  Serial.print(edge);
  Serial.println(F(" OK"));
}

String normalizeVarKey(String key) {
  key.trim();
  if (key.startsWith("$")) key = key.substring(1);
  key.toLowerCase();
  return key;
}

bool validVarKey(const String& key) {
  if (!key.length() || key.indexOf('=') >= 0) return false;
  for (uint16_t i = 0; i < key.length(); i++) {
    char c = key[i];
    bool ok = isAlphaNumeric(c) || c == '_' || c == '-' || c == '.';
    if (!ok) return false;
  }
  return true;
}

void cmdLet(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (argc < 2 || sub == "list") {
    String out = readWholeFile(CONF_STATE);
    Serial.print(out.length() ? out : "(empty)\n");
    return;
  }
  if (sub == "get") {
    if (argc < 3) { Serial.println(F("usage: let get <name>")); return; }
    Serial.println(kvGetFile(CONF_STATE, normalizeVarKey(args[2]), ""));
    return;
  }
  if (sub == "rm" || sub == "unset") {
    if (argc < 3) { Serial.println(F("usage: let rm <name>")); return; }
    Serial.println(kvRemoveFile(CONF_STATE, normalizeVarKey(args[2])) ? F("OK") : F("let: not found"));
    return;
  }
  String key = normalizeVarKey(args[1]);
  uint8_t valueStart = 2;
  if (argc >= 3 && args[2] == "=") valueStart = 3;
  if (!validVarKey(key) || argc <= valueStart) { Serial.println(F("usage: let <name> = <value>")); return; }
  Serial.println(kvSetFile(CONF_STATE, key, joinArgs(args, argc, valueStart)) ? F("OK") : F("let: set failed"));
}

void cmdDefine(String args[], int argc) {
  String cmd = args[0];
  cmd.toLowerCase();
  if (cmd == "undef") {
    if (argc < 2) { Serial.println(F("usage: undef <NAME>")); return; }
    Serial.println(kvRemoveFile(CONF_DEFINES, normalizeVarKey(args[1])) ? F("OK") : F("define: not found"));
    return;
  }
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (argc < 2 || sub == "list") {
    String out = readWholeFile(CONF_DEFINES);
    Serial.print(out.length() ? out : "(empty)\n");
    return;
  }
  if (sub == "rm" || sub == "undef") {
    if (argc < 3) { Serial.println(F("usage: define rm <NAME>")); return; }
    Serial.println(kvRemoveFile(CONF_DEFINES, normalizeVarKey(args[2])) ? F("OK") : F("define: not found"));
    return;
  }
  String key = normalizeVarKey(args[1]);
  uint8_t valueStart = 2;
  if (argc >= 3 && args[2] == "=") valueStart = 3;
  if (!validVarKey(key) || argc <= valueStart) { Serial.println(F("usage: define <NAME> <value>")); return; }
  Serial.println(kvSetFile(CONF_DEFINES, key, joinArgs(args, argc, valueStart)) ? F("OK") : F("define: set failed"));
}

String functionPath(String name) {
  String safe = safeFunctionName(name);
  return safe.length() ? String(FUNC_DIR) + "/" + safe + ".fn" : "";
}

bool functionExists(String name) {
  String path = functionPath(name);
  return path.length() && LittleFS.exists(path) && !isDirectory(path);
}

void runFunctionInline(String name) {
  String path = functionPath(name);
  if (!path.length() || !LittleFS.exists(path)) { Serial.println(F("function: not found")); return; }
  String body = stripOuterBlock(readWholeFile(path));
  body.replace("\r", "\n");
  body.replace("\n", ";");
  String current;
  bool quoted = false;
  char quoteChar = '\0';
  for (uint16_t i = 0; i < body.length(); i++) {
    char c = body[i];
    if ((c == '"' || c == '\'') && (!quoted || quoteChar == c)) {
      quoted = !quoted;
      quoteChar = quoted ? c : '\0';
      current += c;
    } else if (!quoted && c == ';') {
      current = stripCLineComment(current);
      current.trim();
      if (current.length() && !current.startsWith("#") && !current.startsWith("//")) executeLine(current);
      current = "";
      yield();
    } else {
      current += c;
    }
  }
  current = stripCLineComment(current);
  current.trim();
  if (current.length() && !current.startsWith("#") && !current.startsWith("//")) executeLine(current);
}

void runFunctionName(String name) {
  String path = functionPath(name);
  if (!path.length() || !LittleFS.exists(path)) { Serial.println(F("function: not found")); return; }
  runCommandBlock(readWholeFile(path));
}

void cmdFunction(String args[], int argc) {
  if (!ensureFS()) return;
  LittleFS.mkdir(FUNC_DIR);
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (argc < 2 || sub == "list") {
    Dir dir = LittleFS.openDir(FUNC_DIR);
    bool any = false;
    while (dir.next()) {
      String name = basenameOf(dir.fileName());
      if (name.endsWith(".fn")) name.remove(name.length() - 3);
      Serial.println(name);
      any = true;
    }
    if (!any) Serial.println(F("(empty)"));
    return;
  }
  if (sub == "show") {
    if (argc < 3) { Serial.println(F("usage: function show <name>")); return; }
    String path = functionPath(args[2]);
    if (!path.length() || !LittleFS.exists(path)) { Serial.println(F("function: not found")); return; }
    Serial.print(readWholeFile(path));
    return;
  }
  if (sub == "rm" || sub == "remove") {
    if (argc < 3) { Serial.println(F("usage: function rm <name>")); return; }
    String path = functionPath(args[2]);
    Serial.println(path.length() && LittleFS.remove(path) ? F("OK") : F("function: not found"));
    return;
  }
  if (sub == "run" || sub == "call") {
    if (argc < 3) { Serial.println(F("usage: function run <name>")); return; }
    runFunctionName(args[2]);
    return;
  }
  String name = safeFunctionName(args[1]);
  String body = joinArgs(args, argc, 2);
  body.trim();
  if (!name.length() || !body.length()) { Serial.println(F("usage: function <name> { cmd; cmd }")); return; }
  body = stripOuterBlock(body);
  Serial.println(writeWholeFile(functionPath(name), body + "\n") ? F("OK") : F("function: save failed"));
}

void cmdCall(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: call <function>")); return; }
  runFunctionName(args[1]);
}

void cmdRepeat(String args[], int argc) {
  if (argc < 3) { Serial.println(F("usage: repeat <count> <command>")); return; }
  int count = args[1].toInt();
  if (count < 1 || count > 50) { Serial.println(F("repeat: count must be 1..50")); return; }
  String command = joinArgs(args, argc, 2);
  for (int i = 0; i < count; i++) {
    executeLine(command);
    yield();
  }
}

void cmdWatch(String args[], int argc) {
  if (argc < 3) { Serial.println(F("usage: watch <seconds> <command>")); return; }
  unsigned long seconds = args[1].toInt();
  if (seconds < 1) seconds = 1;
  if (seconds > 3600) seconds = 3600;
  executeLine(joinArgs(args, argc, 2));
  Serial.print(F("next in "));
  Serial.print(seconds);
  Serial.println(F("s; use timer every for background repeat"));
}

void cmdPkg(String args[], int argc) {
  if (!ensureFS()) return;
  LittleFS.mkdir(PKG_DIR);
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (sub == "list") {
    Dir dir = LittleFS.openDir(PKG_DIR);
    bool any = false;
    while (dir.next()) {
      any = true;
      Serial.println(basenameOf(dir.fileName()));
    }
    if (!any) Serial.println(F("(empty)"));
  } else if (sub == "add") {
    if (argc < 4) { Serial.println(F("usage: pkg add <name> <script-line>")); return; }
    Serial.println(writeWholeFile(String(PKG_DIR) + "/" + args[2] + ".sh", joinArgs(args, argc, 3) + "\n") ? F("OK") : F("pkg: add failed"));
  } else if (sub == "run") {
    if (argc < 3) { Serial.println(F("usage: pkg run <name>")); return; }
    runScriptFile(String(PKG_DIR) + "/" + args[2] + ".sh");
  } else if (sub == "show") {
    if (argc < 3) { Serial.println(F("usage: pkg show <name>")); return; }
    File file = LittleFS.open(String(PKG_DIR) + "/" + args[2] + ".sh", "r");
    if (!file) { Serial.println(F("pkg: not found")); return; }
    while (file.available()) Serial.write(file.read());
    file.close();
  } else if (sub == "rm" || sub == "remove") {
    if (argc < 3) { Serial.println(F("usage: pkg rm <name>")); return; }
    Serial.println(LittleFS.remove(String(PKG_DIR) + "/" + args[2] + ".sh") ? F("OK") : F("pkg: not found"));
  } else {
    Serial.println(F("usage: pkg list|add|run|show|rm"));
  }
}

void cmdOnBoot(String args[], int argc) {
  if (!ensureFS()) return;
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (sub == "list" || sub == "show") {
    File file = LittleFS.open(DEFAULT_BOOT_SCRIPT, "r");
    if (!file) { Serial.println(F("(empty)")); return; }
    uint8_t n = 1;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() && !line.startsWith("#")) {
        Serial.print(n); Serial.print(F(": ")); Serial.println(line);
      }
      n++;
    }
    file.close();
  } else if (sub == "add") {
    if (argc < 3) { Serial.println(F("usage: onboot add <command>")); return; }
    File file = LittleFS.open(DEFAULT_BOOT_SCRIPT, "a");
    if (!file) { Serial.println(F("onboot: open failed")); return; }
    file.println(joinArgs(args, argc, 2));
    file.close();
    Serial.println(F("OK"));
  } else if (sub == "rm") {
    if (argc < 3) { Serial.println(F("usage: onboot rm <line-number>")); return; }
    uint8_t target = args[2].toInt();
    String out;
    File file = LittleFS.open(DEFAULT_BOOT_SCRIPT, "r");
    if (!file) { Serial.println(F("onboot: open failed")); return; }
    uint8_t n = 1;
    while (file.available()) {
      String line = file.readStringUntil('\n');
      if (n != target) {
        out += line;
        if (!line.endsWith("\n")) out += "\n";
      }
      n++;
    }
    file.close();
    Serial.println(writeWholeFile(DEFAULT_BOOT_SCRIPT, out) ? F("OK") : F("onboot: write failed"));
  } else if (sub == "clear") {
    Serial.println(writeWholeFile(DEFAULT_BOOT_SCRIPT, "# KernelESP boot script\n") ? F("OK") : F("onboot: clear failed"));
  } else {
    Serial.println(F("usage: onboot list|add|rm|clear"));
  }
}

uint8_t freeCronSlots() {
  uint8_t freeSlots = 0;
  for (uint8_t i = 0; i < MAX_CRONS; i++) if (!crons[i].active) freeSlots++;
  return freeSlots;
}

void cmdSchedule(String args[], int argc) {
  if (argc < 4) { Serial.println(F("usage: schedule <relay> <onHH:MM> <offHH:MM>")); return; }
  uint8_t h, m;
  if (!parseClockTime(args[2], h, m) || !parseClockTime(args[3], h, m)) {
    Serial.println(F("schedule: bad time; use HH:MM"));
    return;
  }
  if (freeCronSlots() < 2) { Serial.println(F("schedule: need two free cron slots")); return; }
  executeLine("cron add daily " + args[2] + " relay on " + args[1]);
  executeLine("cron add daily " + args[3] + " relay off " + args[1]);
}

void cmdClimate(String args[], int argc) {
  if (argc < 5) { Serial.println(F("usage: climate temp|hum <relay> <low> <high>")); return; }
  String metric = args[1];
  metric.toLowerCase();
  if (metric != "temp" && metric != "hum") { Serial.println(F("climate: metric must be temp or hum")); return; }
  executeLine("rule add " + metric + " range " + args[3] + " " + args[4] + " relay " + args[2]);
}

String procText(const String& path) {
  if (path == "/proc/meminfo") {
    return "MemFree: " + String(ESP.getFreeHeap()) + "\nMaxBlock: " + String(ESP.getMaxFreeBlockSize()) +
           "\nFragmentation: " + String(ESP.getHeapFragmentation()) + "%\n";
  }
  if (path == "/proc/uptime") return String(millis() / 1000) + "\n";
  if (path == "/proc/wifi") {
    return "status: " + String(WiFi.status()) + "\nssid: " + WiFi.SSID() +
           "\nip: " + WiFi.localIP().toString() + "\n";
  }
  if (path == "/proc/version") return String(KERNEL_NAME) + " " + KERNEL_VERSION + "\n";
  if (path == "/proc/filesystems") return "littlefs\nproc\n";
  if (path == "/proc/flash") {
    return "real_size: " + String(ESP.getFlashChipRealSize()) + "\nide_size: " + String(ESP.getFlashChipSize()) +
           "\nspeed: " + String(ESP.getFlashChipSpeed()) + "\n";
  }
  if (path == "/proc/relays") {
    String out;
    for (uint8_t i = 0; i < MAX_RELAYS; i++) {
      if (!relays[i].configured) continue;
      out += relays[i].name + " GPIO" + String(relays[i].pin) + " " + (relays[i].state ? "on" : "off") + "\n";
    }
    if (!out.length()) out = "(none)\n";
    return out;
  }
  return "";
}

void cmdDryRun(String args[], int argc) {
  if (argc < 2 || args[1] == "status") {
    Serial.println(configGetValue("dryrun", "off"));
    return;
  }
  String value = args[1];
  value.toLowerCase();
  if (value != "on" && value != "off") { Serial.println(F("usage: dryrun on|off|status")); return; }
  Serial.println(configSetValue("dryrun", value) ? F("OK") : F("dryrun: config failed"));
}

void cmdArm(String args[], int argc) {
  (void)argc;
  String cmd = args[0];
  cmd.toLowerCase();
  if (cmd == "armed") {
    Serial.println(automationsArmed ? F("on") : F("off"));
    return;
  }
  automationsArmed = cmd == "arm";
  Serial.println(configSetValue("system.armed", automationsArmed ? "on" : "off") ? F("OK") : F("arm: config failed"));
}

void cmdIfconfig() {
  Serial.println(F("wlan0: flags=UP,BROADCAST,RUNNING"));
  Serial.print(F("  inet ")); Serial.println(WiFi.localIP());
  Serial.print(F("  netmask ")); Serial.println(WiFi.subnetMask());
  Serial.print(F("  gateway ")); Serial.println(WiFi.gatewayIP());
  Serial.print(F("  ether ")); Serial.println(WiFi.macAddress());
  Serial.print(F("  ssid ")); Serial.println(WiFi.SSID());
  Serial.print(F("  status ")); Serial.println(WiFi.status() == WL_CONNECTED ? F("connected") : (wifiConnecting ? F("connecting") : F("not connected")));
  Serial.print(F("ap0: "));
  Serial.println(fallbackApRunning ? WiFi.softAPIP().toString() : "off");
}

void cmdIp(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "addr";
  sub.toLowerCase();
  if (sub == "addr" || sub == "a") {
    Serial.println(F("1: lo: <LOOPBACK,UP>"));
    Serial.println(F("    inet 127.0.0.1/8"));
    Serial.println(F("2: wlan0: <BROADCAST,UP>"));
    Serial.print(F("    inet ")); Serial.println(WiFi.localIP());
    Serial.print(F("    link/ether ")); Serial.println(WiFi.macAddress());
    if (fallbackApRunning) {
      Serial.println(F("3: ap0: <BROADCAST,UP>"));
      Serial.print(F("    inet ")); Serial.println(WiFi.softAPIP());
    }
  } else if (sub == "route" || sub == "r") {
    Serial.print(F("default via ")); Serial.println(WiFi.gatewayIP());
    Serial.print(F("dns ")); Serial.println(WiFi.dnsIP());
  } else {
    Serial.println(F("usage: ip addr|route"));
  }
}

void cmdCrontab(String args[], int argc) {
  if (argc < 2 || args[1] == "-l" || args[1] == "list") {
    Serial.print(cronsText());
  } else if (args[1] == "-r" || args[1] == "clear") {
    String cronArgs[] = {"cron", "clear"};
    cmdCron(cronArgs, 2);
  } else if (args[1] == "add") {
    executeLine("cron add " + joinArgs(args, argc, 2));
  } else {
    Serial.println(F("usage: crontab -l|-r|add <cron args>"));
  }
}

void cmdSystemctl(String args[], int argc) {
  if (argc < 2 || args[1] == "list") {
    Serial.println(F("web ntp sensor wifi"));
    return;
  }
  String action = args[1];
  String name = argc >= 3 ? args[2] : "";
  action.toLowerCase();
  name.toLowerCase();
  if (action != "status" && action != "start" && action != "stop" && action != "restart") {
    name = args[1];
    action = argc >= 3 ? args[2] : "status";
    name.toLowerCase();
    action.toLowerCase();
  }
  if (!name.length()) { Serial.println(F("usage: systemctl status|start|stop|restart <service>")); return; }
  executeLine("service " + name + " " + action);
}

bool jobTextMatches(const String& text, const String& pattern) {
  String a = text;
  String b = pattern;
  a.toLowerCase();
  b.toLowerCase();
  return a.indexOf(b) >= 0;
}

void printPseudoJob(uint16_t pid, const __FlashStringHelper* type, uint8_t id, const String& command, bool verbose) {
  Serial.print(pid);
  if (verbose) {
    Serial.print(' ');
    Serial.print(type);
    Serial.print(':');
    Serial.print(id);
    Serial.print(F(" "));
    Serial.print(command);
  }
  Serial.println();
}

void cmdPgrep(String args[], int argc, bool pidOnly) {
  bool verbose = false;
  uint8_t start = 1;
  if (!pidOnly && argc >= 2 && args[1] == "-a") {
    verbose = true;
    start = 2;
  }
  if (argc <= start) { Serial.println(pidOnly ? F("usage: pidof <text>") : F("usage: pgrep [-a] <text>")); return; }
  String pattern = joinArgs(args, argc, start);
  bool any = false;
  for (uint8_t i = 0; i < MAX_TIMERS; i++) {
    if (timers[i].active && jobTextMatches(timers[i].command, pattern)) {
      any = true;
      printPseudoJob(100 + timers[i].id, F("timer"), timers[i].id, timers[i].command, verbose);
    }
  }
  for (uint8_t i = 0; i < MAX_RULES; i++) {
    if (!rules[i].active) continue;
    String text = rules[i].command + " " + rules[i].offCommand;
    if (jobTextMatches(text, pattern)) {
      any = true;
      printPseudoJob(200 + rules[i].id, F("rule"), rules[i].id, text, verbose);
    }
  }
  for (uint8_t i = 0; i < MAX_CRONS; i++) {
    if (crons[i].active && jobTextMatches(crons[i].command, pattern)) {
      any = true;
      printPseudoJob(300 + crons[i].id, F("cron"), crons[i].id, crons[i].command, verbose);
    }
  }
  if (!any) Serial.println(F("(none)"));
}

void cmdKill(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: kill <pseudo-pid>")); return; }
  uint16_t pid = args[1].toInt();
  if (pid >= 100 && pid < 200) {
    uint8_t id = pid - 100;
    for (uint8_t i = 0; i < MAX_TIMERS; i++) if (timers[i].active && timers[i].id == id) {
      timers[i] = TimerJob();
      saveTimers();
      Serial.println(F("OK"));
      return;
    }
  } else if (pid >= 200 && pid < 300) {
    uint8_t id = pid - 200;
    for (uint8_t i = 0; i < MAX_RULES; i++) if (rules[i].active && rules[i].id == id) {
      rules[i] = Rule();
      saveRules();
      Serial.println(F("OK"));
      return;
    }
  } else if (pid >= 300 && pid < 400) {
    uint8_t id = pid - 300;
    for (uint8_t i = 0; i < MAX_CRONS; i++) if (crons[i].active && crons[i].id == id) {
      crons[i] = CronJob();
      saveCrons();
      Serial.println(F("OK"));
      return;
    }
  }
  Serial.println(F("kill: not found"));
}

void cmdStat(String args[], int argc) {
  if (argc < 2) { Serial.println(F("usage: stat <path>")); return; }
  String path = normalizePath(args[1]);
  if (path.startsWith("/proc/")) {
    String out = procText(path);
    if (!out.length()) { Serial.println(F("stat: not found")); return; }
    Serial.println(F("Type: pseudo-file"));
    Serial.print(F("Size: ")); Serial.println(out.length());
    Serial.print(F("Path: ")); Serial.println(path);
    return;
  }
  if (path == "/proc") {
    Serial.println(F("Type: pseudo-directory"));
    Serial.println(F("Path: /proc"));
    return;
  }
  if (!pathExists(path)) { Serial.println(F("stat: not found")); return; }
  Serial.print(F("Path: ")); Serial.println(path);
  if (isDirectory(path)) {
    Serial.println(F("Type: directory"));
    return;
  }
  File file = LittleFS.open(path, "r");
  if (!file) { Serial.println(F("stat: cannot open")); return; }
  Serial.println(F("Type: file"));
  Serial.print(F("Size: ")); Serial.println(file.size());
  file.close();
}

void cmdExport(String args[], int argc) {
  String what = argc >= 2 ? args[1] : "all";
  what.toLowerCase();
  if (what == "all") Serial.print(backupText());
  else if (what == "relays") Serial.print(readWholeFile(CONF_RELAYS));
  else if (what == "rules") Serial.print(readWholeFile(CONF_RULES));
  else if (what == "cron" || what == "crons") Serial.print(readWholeFile(CONF_CRONS));
  else if (what == "inputs") Serial.print(readWholeFile(CONF_INPUTS));
  else if (what == "scenes") Serial.print(readWholeFile(CONF_SCENES));
  else if (what == "config") Serial.print(readWholeFile(CONF_CONFIG));
  else if (what == "aliases") Serial.print(readWholeFile(CONF_ALIASES));
  else Serial.println(F("usage: export all|config|relays|rules|cron|inputs|scenes|aliases"));
}

void cmdProfile(String args[], int argc) {
  if (!ensureFS()) return;
  LittleFS.mkdir(PROFILE_DIR);
  String sub = argc >= 2 ? args[1] : "list";
  sub.toLowerCase();
  if (sub == "list") {
    Dir dir = LittleFS.openDir(PROFILE_DIR);
    bool any = false;
    while (dir.next()) {
      if (dir.isDirectory()) continue;
      String name = basenameOf(dir.fileName());
      if (!name.endsWith(".bak")) continue;
      name.remove(name.length() - 4);
      any = true;
      Serial.println(name);
    }
    if (!any) Serial.println(F("(empty)"));
  } else if (sub == "save") {
    if (argc < 3) { Serial.println(F("usage: profile save <name>")); return; }
    Serial.println(writeWholeFile(String(PROFILE_DIR) + "/" + args[2] + ".bak", backupText(false)) ? F("OK") : F("profile: save failed"));
  } else if (sub == "show") {
    if (argc < 3) { Serial.println(F("usage: profile show <name>")); return; }
    String path = String(PROFILE_DIR) + "/" + args[2] + ".bak";
    File file = LittleFS.open(path, "r");
    if (!file) { Serial.println(F("profile: not found")); return; }
    while (file.available()) Serial.write(file.read());
    file.close();
  } else if (sub == "load") {
    if (argc < 3) { Serial.println(F("usage: profile load <name> --yes")); return; }
    if (argc < 4 || args[3] != "--yes") {
      Serial.println(F("profile: use --yes to confirm load"));
      return;
    }
    uint8_t restored = 0;
    if (!restoreBackupText(readWholeFile(String(PROFILE_DIR) + "/" + args[2] + ".bak"), restored)) {
      Serial.println(F("profile: restore failed"));
      return;
    }
    loadRelays(); loadTimers(); loadRules(); loadCrons(); loadInputs(); loadAliases();
    Serial.print(F("restored: ")); Serial.println(restored);
  } else if (sub == "rm" || sub == "remove") {
    if (argc < 3) { Serial.println(F("usage: profile rm <name>")); return; }
    Serial.println(LittleFS.remove(String(PROFILE_DIR) + "/" + args[2] + ".bak") ? F("OK") : F("profile: not found"));
  } else {
    Serial.println(F("usage: profile list|save|load|show|rm"));
  }
}

void printBoardPins(const String& profile) {
  if (profile == "esp01") {
    Serial.println(F("profile: esp01"));
    Serial.println(F("safe: GPIO2 after boot"));
    Serial.println(F("risky: GPIO0 GPIO1/TX GPIO3/RX"));
    Serial.println(F("i2c: external wiring required"));
    Serial.println(F("relay: use tested ESP-01 relay adapter"));
    return;
  }
  if (profile == "esp12f") {
    Serial.println(F("profile: esp12f"));
    Serial.println(F("safe: GPIO4 GPIO5 GPIO12 GPIO13 GPIO14"));
    Serial.println(F("risky: GPIO0 GPIO2 GPIO15 GPIO16 RX TX GPIO6-GPIO11"));
    Serial.println(F("i2c: GPIO4=SDA GPIO5=SCL"));
    Serial.println(F("relay: prefer GPIO5/GPIO12/GPIO13/GPIO14; avoid boot pins"));
    return;
  }
  Serial.print(F("profile: ")); Serial.println(profile.length() ? profile : "generic");
  Serial.println(F("safe: D1 D2 D5 D6 D7"));
  Serial.println(F("risky: D0 D3 D4 D8 RX TX GPIO6-GPIO11"));
  Serial.println(F("i2c: D2=SDA D1=SCL"));
  Serial.println(F("relay: active_low is common; verify with relay pulse"));
}

void cmdBoard(String args[], int argc) {
  String sub = argc >= 2 ? args[1] : "show";
  sub.toLowerCase();
  if (sub == "list") {
    Serial.println(F("generic"));
    Serial.println(F("nodemcu"));
    Serial.println(F("d1mini"));
    Serial.println(F("esp12f"));
    Serial.println(F("esp01"));
  } else if (sub == "use") {
    if (argc < 3) { Serial.println(F("usage: board use generic|nodemcu|d1mini|esp12f|esp01")); return; }
    String profile = args[2];
    profile.toLowerCase();
    if (profile != "generic" && profile != "nodemcu" && profile != "d1mini" && profile != "esp12f" && profile != "esp01") {
      Serial.println(F("board: unknown profile"));
      return;
    }
    Serial.println(configSetValue("board.profile", profile) ? F("OK") : F("board: save failed"));
  } else if (sub == "pins" || sub == "show" || sub == "status") {
    printBoardPins(configGetValue("board.profile", "generic"));
  } else {
    Serial.println(F("usage: board list|show|pins|use <profile>"));
  }
}

void cmdDiag() {
  Serial.println(F("== version =="));
  Serial.println(F(KERNEL_NAME " " KERNEL_VERSION));
  Serial.println(F("== board =="));
  printBoardPins(configGetValue("board.profile", "generic"));
  Serial.println(F("== health =="));
  Serial.print(healthText());
  Serial.println(F("== df =="));
  cmdDf();
  Serial.println(F("== wifi =="));
  Serial.print(wifiStatusText());
  Serial.println(F("== time =="));
  Serial.print(timeStatusText());
  Serial.println(F("== sensor =="));
  Serial.print(sensorText());
  Serial.println(F("== relays =="));
  Serial.print(relayStatusText());
  Serial.println(F("== rules =="));
  Serial.print(rulesText());
  Serial.println(F("== cron =="));
  Serial.print(cronsText());
  Serial.println(F("== timers =="));
  String timerArgs[] = {"timer", "list"};
  cmdTimer(timerArgs, 2);
  Serial.println(F("== inputs =="));
  Serial.print(inputsText());
  Serial.println(F("== dmesg =="));
  cmdDmesg();
}

void executeLine(String line) {
  line = stripCLineComment(line);
  line.trim();
  if (!line.length()) return;
  if (line.startsWith("//")) return;

  String args[MAX_ARGS];
  int argc = splitArgs(line, args, MAX_ARGS);
  if (argc == 0) return;
  String cmd = args[0];
  cmd.toLowerCase();

  for (uint8_t i = 0; i < MAX_ALIASES; i++) {
    if (aliases[i].active && aliases[i].name == cmd) {
      String expanded = aliases[i].command;
      if (argc > 1) expanded += " " + joinArgs(args, argc, 1);
      executeLine(expanded);
      return;
    }
  }

  if (hasPipeOutsideQuotes(line)) {
    if (!suppressHistory) addHistory(line);
    executePipeline(line);
    return;
  }

  if (!suppressHistory) addHistory(line);

  if (cmd == "help" || cmd == "man" || cmd == "?") cmdHelp(args, argc);
  else if (cmd == "version") Serial.println(F(KERNEL_NAME " " KERNEL_VERSION));
  else if (cmd == "clear") for (uint8_t i = 0; i < 40; i++) Serial.println();
  else if (cmd == "echo") Serial.println(joinArgs(args, argc, 1));
  else if (cmd == "history") cmdHistory(args, argc);
  else if (cmd == "arm" || cmd == "disarm" || cmd == "armed") cmdArm(args, argc);
  else if (cmd == "id") Serial.println(F("uid=0(root) gid=0(root) groups=0(root)"));
  else if (cmd == "groups") Serial.println(F("root"));
  else if (cmd == "who") Serial.println(F("root ttyS0"));
  else if (cmd == "w") {
    Serial.print(F(" ")); Serial.print(uptimeText(true)); Serial.print(F(", heap ")); Serial.println(ESP.getFreeHeap());
    Serial.println(F("USER TTY   WHAT"));
    Serial.println(F("root ttyS0 shell"));
  }
  else if (cmd == "sync") Serial.println(F("OK"));
  else if (cmd == "true") Serial.println(F("true"));
  else if (cmd == "false") Serial.println(F("false"));
  else if (cmd == "test" || cmd == "[") cmdTest(args, argc);
  else if (cmd == "if") cmdIf(args, argc);
  else if (cmd == "when") cmdWhen(args, argc);
  else if (cmd == "let" || cmd == "var") cmdLet(args, argc);
  else if (cmd == "define" || cmd == "undef") cmdDefine(args, argc);
  else if (cmd == "function") cmdFunction(args, argc);
  else if (cmd == "call") cmdCall(args, argc);
  else if (cmd == "basename" || cmd == "dirname") cmdPathTool(cmd, args, argc);
  else if (cmd == "repeat") cmdRepeat(args, argc);
  else if (cmd == "watch") cmdWatch(args, argc);
  else if (cmd == "uname") {
    Serial.println(F(KERNEL_NAME " " KERNEL_VERSION));
    Serial.println(F("arch: esp8266"));
    Serial.print(F("board: ")); Serial.println(configGetValue("board.profile", "generic"));
    Serial.println(F("fs: LittleFS"));
  }
  else if (cmd == "uptime") {
    Serial.println(argc >= 2 && args[1] == "-p" ? uptimeText(true) : uptimeText(false));
  }
  else if (cmd == "free" || cmd == "heap") {
    Serial.print(F("free heap: ")); Serial.println(ESP.getFreeHeap());
    Serial.print(F("heap frag: ")); Serial.print(ESP.getHeapFragmentation()); Serial.println('%');
    Serial.print(F("max block: ")); Serial.println(ESP.getMaxFreeBlockSize());
  }
  else if (cmd == "ps" || cmd == "top") {
    Serial.println(F("PID  NAME       STATE"));
    Serial.println(F("1    shell      running"));
    Serial.println(F("2    wifi       idle"));
    Serial.println(F("3    littlefs   mounted"));
    for (uint8_t i = 0; i < MAX_TIMERS; i++) if (timers[i].active) {
      Serial.print(100 + timers[i].id); Serial.println(F("  timer      armed"));
    }
    for (uint8_t i = 0; i < MAX_RULES; i++) if (rules[i].active) {
      Serial.print(200 + rules[i].id); Serial.println(F("  rule       armed"));
    }
    for (uint8_t i = 0; i < MAX_CRONS; i++) if (crons[i].active) {
      Serial.print(300 + crons[i].id); Serial.println(F("  cron       armed"));
    }
  }
  else if (cmd == "pgrep") cmdPgrep(args, argc, false);
  else if (cmd == "pidof") cmdPgrep(args, argc, true);
  else if (cmd == "kill") cmdKill(args, argc);
  else if (cmd == "dmesg") cmdDmesg();
  else if (cmd == "reboot") { Serial.println(F("rebooting")); delay(250); ESP.restart(); }
  else if (cmd == "sleep") { if (argc > 1) delay(args[1].toInt()); Serial.println(F("OK")); }
  else if (cmd == "mem" || cmd == "resetreason" || cmd == "chip" || cmd == "flash" || cmd == "sysinfo") cmdSysinfo(cmd);
  else if (cmd == "relay") cmdRelay(args, argc);
  else if (cmd == "timer") cmdTimer(args, argc);
  else if (cmd == "rule") cmdRule(args, argc);
  else if (cmd == "cron") cmdCron(args, argc);
  else if (cmd == "config") cmdConfig(args, argc);
  else if (cmd == "board") cmdBoard(args, argc);
  else if (cmd == "diag") {
    if (argc >= 2 && args[1] == "wifi") cmdDiagWifi();
    else cmdDiag();
  }
  else if (cmd == "export") cmdExport(args, argc);
  else if (cmd == "log") cmdLog(args, argc);
  else if (cmd == "journalctl") {
    if (argc >= 2 && (args[1] == "-n" || args[1] == "--lines")) {
      String logArgs[] = {"log", "tail", argc >= 3 ? args[2] : "10"};
      cmdLog(logArgs, 3);
    } else cmdDmesg();
  }
  else if (cmd == "logger") cmdLogger(args, argc);
  else if (cmd == "pkg") cmdPkg(args, argc);
  else if (cmd == "profile") cmdProfile(args, argc);
  else if (cmd == "dryrun") cmdDryRun(args, argc);
  else if (cmd == "schedule") cmdSchedule(args, argc);
  else if (cmd == "climate") cmdClimate(args, argc);
  else if (cmd == "web") cmdWeb(args, argc);
  else if (cmd == "safe") cmdSafe(args, argc);
  else if (cmd == "health") cmdHealth(args, argc);
  else if (cmd == "service") cmdService(args, argc);
  else if (cmd == "systemctl") cmdSystemctl(args, argc);
  else if (cmd == "date") cmdDate(args, argc);
  else if (cmd == "time" || cmd == "ntp") cmdTimeNet(args, argc);
  else if (cmd == "mail") cmdMail(args, argc);
  else if (cmd == "ping") cmdPing(args, argc);
  else if (cmd == "httpget") cmdHttpGet(args, argc);
  else if (cmd == "hostname") cmdHostname(args, argc);
  else if (cmd == "whoami") Serial.println(F("root"));
  else if (cmd == "which") cmdWhich(args, argc);
  else if (cmd == "mount") cmdMount();
  else if (cmd == "jobs") cmdJobs();
  else if (cmd == "motd") cmdMotd(args, argc);
  else if (cmd == "state") cmdState(args, argc);
  else if (cmd == "scene") cmdScene(args, argc);
  else if (cmd == "input") cmdInput(args, argc);
  else if (cmd == "sensor") cmdSensor(args, argc);
  else if (cmd == "i2c") cmdI2c(args, argc);
  else if (cmd == "pcf") cmdPcf(args, argc);
  else if (cmd == "mcp") cmdMcp(args, argc);
  else if (cmd == "backup") Serial.print(backupText());
  else if (cmd == "restore") cmdRestore(args, argc);
  else if (cmd == "sh") {
    if (argc < 2) { Serial.println(F("usage: sh [-n] <script>")); return; }
    if (args[1] == "-n") {
      if (argc < 3) { Serial.println(F("usage: sh -n <script>")); return; }
      validateScriptFile(args[2]);
    } else {
      runScriptFile(args[1]);
    }
  }
  else if (cmd == "source" || cmd == "run") {
    if (argc < 2) { Serial.println(cmd == "source" ? F("usage: source <script>") : F("usage: run <script>")); return; }
    runScriptFile(args[1]);
  }
  else if (cmd == "boot") cmdBoot(args, argc);
  else if (cmd == "onboot") cmdOnBoot(args, argc);
  else if (cmd == "alias") cmdAlias(args, argc);
  else if (cmd == "unalias") {
    if (argc < 2) { Serial.println(F("usage: unalias <name>")); return; }
    for (uint8_t i = 0; i < MAX_ALIASES; i++) {
      if (aliases[i].active && aliases[i].name == args[1]) {
        aliases[i] = AliasEntry();
        Serial.println(F("OK"));
        return;
      }
    }
    Serial.println(F("unalias: not found"));
  }
  else if (cmd == "env" || cmd == "printenv") cmdEnv(args, argc, false, false);
  else if (cmd == "set" || cmd == "setenv") cmdEnv(args, argc, true, false);
  else if (cmd == "unset") cmdEnv(args, argc, false, true);
  else if (cmd == "pwd") Serial.println(cwd);
  else if (cmd == "cd") {
    String path = argc > 1 ? normalizePath(args[1]) : "/";
    if (path == "/" || isDirectory(path)) { cwd = path; Serial.println(cwd); }
    else Serial.println(F("cd: not a directory"));
  }
  else if (cmd == "ls") cmdLs(args, argc);
  else if (cmd == "cat") cmdCat(args, argc);
  else if (cmd == "head") cmdHead(args, argc);
  else if (cmd == "tail") cmdTail(args, argc);
  else if (cmd == "grep") cmdGrep(args, argc);
  else if (cmd == "find") cmdFind(args, argc);
  else if (cmd == "wc") cmdWc(args, argc);
  else if (cmd == "du") cmdDu(args, argc);
  else if (cmd == "stat") cmdStat(args, argc);
  else if (cmd == "touch") {
    if (!ensureFS()) return;
    if (argc < 2) { Serial.println(F("usage: touch <file>")); return; }
    String path = normalizePath(args[1]);
    if (!parentDirectoryExists(path)) { Serial.println(F("touch: parent directory not found")); return; }
    if (isDirectory(path)) { Serial.println(F("touch: target is a directory")); return; }
    File f = LittleFS.open(path, "a");
    if (!f) Serial.println(F("touch: failed")); else { f.close(); Serial.println(F("OK")); }
  }
  else if (cmd == "write") writeFileCommand(args, argc, false);
  else if (cmd == "append") writeFileCommand(args, argc, true);
  else if (cmd == "rm") {
    if (!ensureFS()) return;
    if (argc < 2) { Serial.println(F("usage: rm <file>")); return; }
    String path = normalizePath(args[1]);
    if (isDirectory(path)) { Serial.println(F("rm: target is a directory")); return; }
    Serial.println(LittleFS.remove(path) ? F("OK") : F("rm: failed"));
  }
  else if (cmd == "mkdir") {
    if (!ensureFS()) return;
    if (argc < 2) { Serial.println(F("usage: mkdir <dir>")); return; }
    String path = normalizePath(args[1]);
    if (pathExists(path)) { Serial.println(F("mkdir: already exists")); return; }
    if (!parentDirectoryExists(path)) { Serial.println(F("mkdir: parent directory not found")); return; }
    Serial.println(LittleFS.mkdir(path) ? F("OK") : F("mkdir: failed"));
  }
  else if (cmd == "rmdir") {
    if (!ensureFS()) return;
    if (argc < 2) { Serial.println(F("usage: rmdir <dir>")); return; }
    String path = normalizePath(args[1]);
    if (path == "/") { Serial.println(F("rmdir: refusing to remove /")); return; }
    if (!isDirectory(path)) { Serial.println(F("rmdir: not a directory")); return; }
    Serial.println(LittleFS.rmdir(path) ? F("OK") : F("rmdir: failed"));
  }
  else if (cmd == "cp" || cmd == "mv") {
    if (!ensureFS()) return;
    bool recursive = cmd == "cp" && argc >= 4 && args[1] == "-r";
    if ((!recursive && argc < 3) || (recursive && argc < 4)) { Serial.println(F("usage: cp [-r] <src> <dst> | mv <src> <dst>")); return; }
    String srcPath = normalizePath(args[recursive ? 2 : 1]);
    String dstPath = normalizePath(args[recursive ? 3 : 2]);
    if (isDirectory(srcPath)) {
      if (recursive) {
        Serial.println(copyDirPath(srcPath, dstPath, 0) ? F("OK") : F("copy: recursive failed"));
        return;
      }
      Serial.println(F("copy: source is a directory"));
      return;
    }
    if (!LittleFS.exists(srcPath)) { Serial.println(F("copy: source not found")); return; }
    if (!copyFilePath(srcPath, dstPath)) { Serial.println(F("copy: failed")); return; }
    if (cmd == "mv" && !LittleFS.remove(srcPath)) { Serial.println(F("mv: copied but source remove failed")); return; }
    Serial.println(F("OK"));
  }
  else if (cmd == "df") cmdDf();
  else if (cmd == "fsformat") {
    if (argc < 2 || args[1] != "--yes") {
      Serial.println(F("fsformat: use fsformat --yes to erase LittleFS"));
      return;
    }
    Serial.println(F("formatting LittleFS..."));
    LittleFS.end();
    Serial.println(LittleFS.format() ? F("OK") : F("format failed"));
    fsReady = LittleFS.begin();
    cwd = "/";
  }
  else if (cmd == "pins") cmdPins();
  else if (cmd == "pinmode") {
    if (argc < 3) { Serial.println(F("usage: pinmode <pin> in|out|pullup")); return; }
    int pin;
    if (!resolvePin(args[1], pin)) return;
    String mode = args[2]; mode.toLowerCase();
    if (mode == "out") pinMode(pin, OUTPUT);
    else if (mode == "in") pinMode(pin, INPUT);
    else if (mode == "pullup") pinMode(pin, INPUT_PULLUP);
    else { Serial.println(F("pinmode: mode must be in, out, or pullup")); return; }
    Serial.println(F("OK"));
  }
  else if (cmd == "gpio" || cmd == "writepin") {
    if (argc < 3) { Serial.println(F("usage: gpio <pin> on|off|toggle")); return; }
    int pin;
    if (!resolvePin(args[1], pin)) return;
    String action = args[2]; action.toLowerCase();
    pinMode(pin, OUTPUT);
    if (configGetValue("dryrun", "off") == "on") {
      Serial.println(F("[dryrun] GPIO write skipped"));
    } else if (action == "toggle") digitalWrite(pin, !digitalRead(pin));
    else if (action == "on" || action == "high" || action == "1") digitalWrite(pin, HIGH);
    else if (action == "off" || action == "low" || action == "0") digitalWrite(pin, LOW);
    else { Serial.println(F("gpio: action must be on, off, high, low, 1, 0, or toggle")); return; }
    Serial.println(F("OK"));
  }
  else if (cmd == "read") {
    if (argc < 2) { Serial.println(F("usage: read <pin>")); return; }
    int pin;
    if (!resolvePin(args[1], pin)) return;
    pinMode(pin, INPUT);
    Serial.println(digitalRead(pin));
  }
  else if (cmd == "toggle") {
    if (argc < 2) { Serial.println(F("usage: toggle <pin>")); return; }
    int pin;
    if (!resolvePin(args[1], pin)) return;
    pinMode(pin, OUTPUT);
    if (configGetValue("dryrun", "off") == "on") Serial.println(F("[dryrun] GPIO toggle skipped"));
    else digitalWrite(pin, !digitalRead(pin));
    Serial.println(F("OK"));
  }
  else if (cmd == "pwm") {
    if (argc < 3) { Serial.println(F("usage: pwm <pin> <0-1023>")); return; }
    int pin;
    int value = constrain(args[2].toInt(), 0, 1023);
    if (!resolvePin(args[1], pin)) return;
    if (pin == 16) { Serial.println(F("pwm: GPIO16 does not support PWM")); return; }
    if (configGetValue("dryrun", "off") == "on") Serial.println(F("[dryrun] PWM write skipped"));
    else analogWrite(pin, value);
    Serial.println(F("OK"));
  }
  else if (cmd == "adc") Serial.println(analogRead(A0));
  else if (cmd == "ap") cmdAp(args, argc);
  else if (cmd == "ifconfig") cmdIfconfig();
  else if (cmd == "ip") cmdIp(args, argc);
  else if (cmd == "wifi") cmdWifi(args, argc);
  else if (cmd == "crontab") cmdCrontab(args, argc);
  else if (functionExists(cmd)) runFunctionName(cmd);
  else Serial.println(F("unknown command; try help"));
}

void printPrompt() {
  Serial.print(F("root@esp8266:"));
  Serial.print(cwd);
  Serial.print(F("# "));
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.setTimeout(50);
  delay(400);
  Serial.println();
  Serial.println(F(KERNEL_NAME " " KERNEL_VERSION));
  Serial.println(F("booting..."));
  analogWriteRange(1023);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  setupWifiEventHandlers();
  WiFi.mode(WIFI_STA);
  applyWifiPhyMode();
  applyWifiOutputPower();
  disconnectWifiStation(false);
  fsReady = LittleFS.begin();
  addLog("boot");
  addLog(fsReady ? "LittleFS mounted" : "LittleFS mount failed");
  ensureSystemDirs();
  ensureUnixDefaults();
  migrateConfigIfNeeded();
  ensureWebAssets();
  automationsArmed = configGetValue("system.armed", "on") == "on";
  eventLog("boot");

  Serial.println(F("type 'help' for commands"));
  if (fsReady && LittleFS.exists(MOTD_FILE)) Serial.print(readWholeFile(MOTD_FILE));
  bool safeBoot = configGetValue("boot.safe", "on") == "on" || LittleFS.exists(SAFE_BOOT_FILE);
  String safePinToken = configGetValue("boot.safepin", "");
  if (safePinToken.length()) {
    int safePin = pinFromToken(safePinToken);
    if (safePin >= 0 && safePin <= 16) {
      pinMode(safePin, INPUT_PULLUP);
      delay(20);
      if (digitalRead(safePin) == LOW) safeBoot = true;
    }
  }
  if (LittleFS.exists(SAFE_BOOT_FILE)) LittleFS.remove(SAFE_BOOT_FILE);
  if (!safeBoot) {
    loadRelays();
    loadTimers();
    loadRules();
    loadCrons();
    loadInputs();
    loadAliases();
    loadHistory();
    if (configGetValue("sensor.autostart", "off") == "on") sensorAutoBegin();
    connectSavedWifi();
    if (configGetValue("web.autostart", "off") == "on") startWeb();
    String bootScript = configGetValue("boot.script", "");
    if (bootScript.length()) runScriptFile(bootScript);
  } else {
    Serial.println(F("safe boot: autorun disabled"));
  }
  bootFinished = true;
  printPrompt();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (inputLen > 0) {
        inputLine[inputLen] = '\0';
        Serial.println();
        executeLine(String(inputLine));
        inputLen = 0;
        memset(inputLine, 0, sizeof(inputLine));
        printPrompt();
      }
    } else if (c == 8 || c == 127) {
      if (inputLen > 0) {
        inputLen--;
        inputLine[inputLen] = '\0';
        Serial.print(F("\b \b"));
      }
    } else if (inputLen < MAX_LINE - 1 && isPrintable(c)) {
      inputLine[inputLen++] = c;
      Serial.print(c);
    }
  }
  processWifi();
  if (webRunning) webServer.handleClient();
  if (bootFinished) processRelayPulses();
  if (bootFinished) processNtp();
  if (bootFinished && automationsArmed) {
    processTimers();
    processInputs();
    processRules();
    processCrons();
  }
  if (bootFinished) processHealthGuard();
  yield();
}
