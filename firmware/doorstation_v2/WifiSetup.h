#pragma once
#include <Preferences.h>
#include <WebServer.h>

struct SavedWifi { uint32_t version; char ssid[33]; char password[65]; };
static String configuredSsid;
static String configuredPassword;
static String additionalSsid;
static String additionalPassword;
static bool usingAdditionalWifi = false;
static String lastSuccessfulWifi;
// Nur während physisch aktivierter Einrichtung: erreichbar über AP und Heimnetz.
static WebServer wifiSetupServer(80);
static bool wifiSetupActive = false;
static uint32_t wifiSetupStarted = 0;
static bool wifiRestartPending = false;
static uint32_t wifiRestartAt = 0;
static String wifiSetupToken;
static constexpr uint32_t WIFI_SETUP_DURATION_MS = 10 * 60 * 1000;

static void loadWifiSettings() {
  configuredSsid = "";
  configuredPassword = "";
  Preferences prefs;
  if (!prefs.begin("intercom-wifi", true)) return;
  SavedWifi saved = {};
  bool found = prefs.getBytesLength("credentials") == sizeof(saved) &&
      prefs.getBytes("credentials", &saved, sizeof(saved)) == sizeof(saved);
  SavedWifi additional = {};
  bool foundAdditional = prefs.getBytesLength("additional") == sizeof(additional) &&
      prefs.getBytes("additional", &additional, sizeof(additional)) == sizeof(additional);
  lastSuccessfulWifi = prefs.getString("lastssid", "");
  prefs.end();
  additionalSsid = ""; additionalPassword = "";
  if (foundAdditional && additional.version == 1 && additional.ssid[32] == 0 &&
      additional.password[64] == 0 && additional.ssid[0]) {
    additionalSsid = additional.ssid; additionalPassword = additional.password;
  }
  if (found && saved.version == 1 && saved.ssid[32] == 0 && saved.password[64] == 0 && saved.ssid[0]) {
    configuredSsid = saved.ssid;
    configuredPassword = saved.password;
  }
}

static void rememberSuccessfulWifi() {
  if (WiFi.status() != WL_CONNECTED) return;
  const String active = WiFi.SSID();
  if (active == lastSuccessfulWifi ||
      (active != configuredSsid && active != additionalSsid)) return;
  Preferences prefs;
  if (!prefs.begin("intercom-wifi", false)) return;
  if (prefs.putString("lastssid", active) == active.length()) lastSuccessfulWifi = active;
  prefs.end();
}

static String wifiHtmlEscape(String s) {
  s.replace("&", "&amp;"); s.replace("<", "&lt;"); s.replace(">", "&gt;");
  s.replace("\"", "&quot;"); s.replace("'", "&#39;"); return s;
}

static void wifiSetupPage() {
  wifiSetupServer.sendHeader("Cache-Control", "no-store");
  String html = R"HTML(<!doctype html><html lang="de"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Open Intercom – WLAN</title><style>
body{font:18px sans-serif;background:#111;color:#eee;margin:30px auto;padding:20px;max-width:500px}
label,input,button{display:block;box-sizing:border-box;width:100%;margin:14px 0}
input,button{font:inherit;padding:14px;border-radius:8px}button{background:#3974c8;color:white;border:0}
small{color:#bbb}</style><h1>WLAN einrichten</h1>
<p>WLAN der Türstation auswählen: Bitte den Namen eines 2,4-GHz-WLANs eingeben.</p>
<form method="post" action="/save"><input type="hidden" name="token" value=")HTML";
  html += wifiSetupToken;
  html += "\"><label>WLAN-Name (SSID)<input name=\"ssid\" required maxlength=\"32\" value=\"";
  html += wifiHtmlEscape(configuredSsid);
  html += R"HTML("></label><label>WLAN-Passwort<input type="password" name="password"
required minlength="8" maxlength="64" autocomplete="new-password"></label>
<p><small>Auch bei unverändertem WLAN das Passwort erneut eingeben.
Nach dem Speichern startet die Türstation neu. Falls die Verbindung scheitert,
ist das Einrichtungs-WLAN wieder verfügbar.</small></p>
<button type="submit">Speichern und neu starten</button></form></html>)HTML";
  wifiSetupServer.send(200, "text/html; charset=utf-8", html);
}

static void wifiSetupSave() {
  wifiSetupServer.sendHeader("Cache-Control", "no-store");
  if (wifiRestartPending || !wifiSetupServer.hasArg("token") ||
      wifiSetupServer.arg("token") != wifiSetupToken) {
    wifiSetupServer.send(403, "text/plain; charset=utf-8", "Bitte die Einrichtungsseite neu öffnen."); return;
  }
  String ssid = wifiSetupServer.arg("ssid"), password = wifiSetupServer.arg("password");
  bool validPassword = password.length() >= 8 && password.length() <= 63;
  if (password.length() == 64) {
    validPassword = true;
    for (size_t i = 0; i < password.length(); ++i) if (!isxdigit((unsigned char)password[i])) validPassword = false;
  }
  if (!ssid.length() || ssid.length() > 32 || !validPassword ||
      memchr(ssid.c_str(), 0, ssid.length()) != nullptr ||
      memchr(password.c_str(), 0, password.length()) != nullptr) {
    wifiSetupServer.send(400, "text/plain; charset=utf-8",
        "SSID: 1–32 Bytes. WPA-Passwort: 8–63 Zeichen oder 64 Hex-Zeichen. Bitte zurückgehen und korrigieren."); return;
  }
  SavedWifi saved = {};
  saved.version = 1;
  memcpy(saved.ssid, ssid.c_str(), ssid.length());
  memcpy(saved.password, password.c_str(), password.length());
  Preferences prefs;
  if (!prefs.begin("intercom-wifi", false)) {
    wifiSetupServer.send(500, "text/plain", "Speichern fehlgeschlagen."); return;
  }
  size_t written = prefs.putBytes("credentials", &saved, sizeof(saved));
  prefs.end();
  if (written != sizeof(saved)) { wifiSetupServer.send(500, "text/plain", "Speichern fehlgeschlagen."); return; }
  wifiSetupServer.send(200, "text/html; charset=utf-8",
      "<meta name=viewport content='width=device-width,initial-scale=1'><h1>Gespeichert</h1>"
      "<p>Die Türstation startet jetzt neu. Das Einrichtungs-WLAN wird dabei getrennt.</p>");
  wifiRestartPending = true;
  wifiRestartAt = millis() + 2000;
}


static bool candidateActive = false;
static String candidateSsid, candidatePassword;
static String candidateResult = "idle";
static uint32_t candidateStarted = 0;
static bool candidateTransactional = false;
static bool candidateJoining = false;
static bool candidateConfirmRequested = false;

static WiFiUDP discoveryUdp;

static String jsonQuote(const String& text) {
  String result = "\"";
  for (size_t i=0;i<text.length();++i) {
    unsigned char c=text[i];
    if (c=='"' || c=='\\') { result+='\\'; result+=(char)c; }
    else if (c<32) { char b[7]; snprintf(b,sizeof(b),"\\u%04x",c); result+=b; }
    else result+=(char)c;
  }
  return result+"\"";
}
static void apiWifiInfo() {
  wifiSetupServer.sendHeader("Cache-Control", "no-store");
  wifiSetupServer.send(200,"application/json", "{\"token\":"+jsonQuote(wifiSetupToken)+
      ",\"stationId\":"+jsonQuote(WiFi.macAddress())+",\"coordinated\":true,\"additionalWifi\":true}");
}
// Shared by the token-protected API and local USB maintenance console.
static bool storeAdditionalWifi(const String& ssid, const String& password) {
  if (candidateActive || wifiRestartPending) return false;
  bool valid = password.length() >= 8 && password.length() <= 63;
  if (password.length() == 64) {
    valid = true;
    for (size_t i = 0; i < password.length(); ++i)
      if (!isxdigit((unsigned char)password[i])) valid = false;
  }
  if (!ssid.length() || ssid.length() > 32 || !valid ||
      memchr(ssid.c_str(), 0, ssid.length()) || memchr(password.c_str(), 0, password.length()) ||
      ssid == configuredSsid) return false;
  SavedWifi saved = {}; saved.version = 1;
  memcpy(saved.ssid, ssid.c_str(), ssid.length());
  memcpy(saved.password, password.c_str(), password.length());
  Preferences prefs;
  bool ok = prefs.begin("intercom-wifi", false);
  if (ok) { ok = prefs.putBytes("additional", &saved, sizeof(saved)) == sizeof(saved); prefs.end(); }
  if (ok) {
    additionalSsid = ssid; additionalPassword = password;
    wifiSetupStarted = millis();
  }
  return ok;
}
// Stores a second profile without disconnecting or replacing the active profile.
static void apiWifiAdditional() {
  wifiSetupServer.sendHeader("Cache-Control", "no-store");
  if (!wifiSetupServer.hasArg("token") || wifiSetupServer.arg("token") != wifiSetupToken) {
    wifiSetupServer.send(403, "application/json", "{}"); return;
  }
  String ssid = wifiSetupServer.arg("ssid");
  if (!storeAdditionalWifi(ssid, wifiSetupServer.arg("password"))) {
    wifiSetupServer.send(400, "application/json", "{\"error\":\"profile_not_stored\"}"); return;
  }
  wifiSetupServer.send(200, "application/json", "{\"stored\":true,\"ssid\":" + jsonQuote(ssid) + "}");
}
static bool annSerialCommand(const String& line);  // Announcement.h
// USB only. Never echo incoming data or expose passwords through responses.
static void handleWifiMaintenance() {
  static String line;
  static bool overflow = false;
  static uint32_t receivedAt = 0;
  if (line.length() && (uint32_t)(millis() - receivedAt) > 5000) { line = ""; overflow = false; }
  int budget = 128;
  while (budget-- > 0 && Serial.available()) {
    char c = Serial.read(); receivedAt = millis();
    if (c == '\n') {
      if (!overflow && line.startsWith("WIFI_ADD ")) {
        int separator = line.indexOf('\t', 9);
        bool ok = separator > 9 && storeAdditionalWifi(line.substring(9, separator), line.substring(separator + 1));
        Serial.println(ok ? "WIFI_PROFILE_STORED" : "WIFI_PROFILE_ERROR");
      } else if (!overflow && line.startsWith("WIFI_ONLY ")) {
        String selected = line.substring(10);
        bool ok = !candidateActive && !wifiRestartPending && selected.length() &&
            (selected == configuredSsid || selected == additionalSsid);
        String password = selected == configuredSsid ? configuredPassword : additionalPassword;
        SavedWifi saved = {}; saved.version = 1;
        if (ok) {
          memcpy(saved.ssid, selected.c_str(), selected.length());
          memcpy(saved.password, password.c_str(), password.length());
          Preferences prefs;
          ok = prefs.begin("intercom-wifi", false);
          if (ok) {
            ok = prefs.putBytes("credentials", &saved, sizeof(saved)) == sizeof(saved);
            if (ok && prefs.isKey("additional")) ok = prefs.remove("additional");
            prefs.end();
          }
        }
        if (ok) {
          configuredSsid = selected; configuredPassword = password;
          additionalSsid = ""; additionalPassword = "";
          usingAdditionalWifi = false;
          // Remove the driver's persisted connection before selecting the sole profile.
          WiFi.disconnect(false, true);
          WiFi.begin(configuredSsid.c_str(), configuredPassword.c_str());
        }
        Serial.println(ok ? "WIFI_ONLY_STORED" : "WIFI_PROFILE_ERROR");
      } else if (!overflow && line == "OTA_ACCESS") {
        printOtaAccess();
      } else if (!overflow && line.startsWith("OTA_SET_PASSWORD ")) {
        Serial.println(setOtaPassword(line.substring(17)) ? "OTA_PASSWORD_STORED" : "OTA_PASSWORD_ERROR");
      } else if (!overflow && annSerialCommand(line)) {
      } else if (!overflow && line == "WIFI_PROFILES") {
        Serial.printf("WIFI_PROFILES primary=%s additional=%s active=%s\n",
            configuredSsid.c_str(), additionalSsid.c_str(), WiFi.SSID().c_str());
      }
      line = ""; overflow = false;
    } else if (c != '\r') {
      if (line.length() < 108 && !overflow) line += c;
      else { line = ""; overflow = true; }
    }
  }
}
static void apiWifiProfiles() {
  wifiSetupServer.sendHeader("Cache-Control", "no-store");
  wifiSetupServer.send(200, "application/json", "{\"primary\":" + jsonQuote(configuredSsid) +
      ",\"additional\":" + jsonQuote(additionalSsid) + ",\"active\":" + jsonQuote(WiFi.SSID()) + "}");
}
static void apiWifiScan() {
  int count=WiFi.scanComplete();
  if (count==WIFI_SCAN_FAILED) { WiFi.scanNetworks(true); count=WIFI_SCAN_RUNNING; }
  String body="{\"scanning\":"+String(count<0?"true":"false")+",\"networks\":[";
  for(int i=0;i<count;++i) {
    if(i) body+=",";
    body+="{\"ssid\":"+jsonQuote(WiFi.SSID(i))+",\"rssi\":"+String(WiFi.RSSI(i))+
        ",\"open\":"+String(WiFi.encryptionType(i)==WIFI_AUTH_OPEN?"true":"false")+"}";
  }
  wifiSetupServer.send(200,"application/json",body+"]}");
}
static void apiWifiConnect(bool transactional = false) {
  if(candidateActive || wifiSetupServer.arg("token")!=wifiSetupToken) {
    wifiSetupServer.send(409,"application/json","{\"error\":\"busy_or_invalid_session\"}");return;
  }
  String ssid=wifiSetupServer.arg("ssid"), pass=wifiSetupServer.arg("password");
  bool valid=pass.length()>=8 && pass.length()<=63;
  if(pass.length()==64) { valid=true; for(size_t i=0;i<pass.length();++i) if(!isxdigit((unsigned char)pass[i]))valid=false; }
  if(!ssid.length() || ssid.length()>32 || !valid ||
      memchr(ssid.c_str(),0,ssid.length()) || memchr(pass.c_str(),0,pass.length())) {
    wifiSetupServer.send(400,"application/json","{\"error\":\"invalid_credentials\"}");return;
  }
  candidateSsid=ssid;candidatePassword=pass;candidateResult="connecting";
  candidateTransactional=transactional;candidateJoining=false;candidateConfirmRequested=false;
  candidateStarted=millis();candidateActive=true;wifiSetupStarted=millis();
  // Reply before moving off the old WLAN; give the tablet time to switch.
  wifiSetupServer.send(202,"application/json","{\"status\":\"connecting\"}");
}
static void apiWifiStatus() {
  wifiSetupServer.sendHeader("Cache-Control","no-store");
  wifiSetupServer.send(200,"application/json","{\"status\":"+jsonQuote(candidateResult)+
      ",\"ip\":"+jsonQuote(WiFi.localIP().toString())+",\"stationId\":"+jsonQuote(WiFi.macAddress())+
      ",\"ssid\":"+jsonQuote(WiFi.SSID())+",\"transactional\":"+String(candidateTransactional?"true":"false")+"}");
}
static void apiWifiConfirm() {
  if(wifiSetupServer.arg("token")!=wifiSetupToken) { wifiSetupServer.send(403,"application/json","{}");return; }
  if(candidateResult=="connected" && candidateTransactional) { wifiSetupServer.send(200,"application/json","{\"status\":\"connected\"}");return; }
  if(!candidateActive || !candidateTransactional || candidateResult!="awaiting_confirmation") { wifiSetupServer.send(409,"application/json","{}");return; }
  candidateConfirmRequested=true;
  wifiSetupServer.send(202,"application/json","{\"status\":\"confirming\"}");
}
static void handleWifiCandidate() {
  if(!candidateActive)return;
  if(candidateTransactional && millis()-candidateStarted>=90000 && candidateResult!="connected") {
    candidateResult="failed";candidateActive=false;candidatePassword="";
    WiFi.disconnect(false,false);WiFi.begin(configuredSsid.c_str(),configuredPassword.c_str());return;
  }
  if(!candidateJoining) {
    if(millis()-candidateStarted<2500)return;
    WiFi.disconnect(false,false);WiFi.begin(candidateSsid.c_str(),candidatePassword.c_str());candidateJoining=true;
  }
  if(WiFi.status()==WL_CONNECTED && WiFi.SSID()==candidateSsid) {
    if(candidateTransactional && !candidateConfirmRequested) {candidateResult="awaiting_confirmation";return;}
    SavedWifi saved={};saved.version=1;
    memcpy(saved.ssid,candidateSsid.c_str(),candidateSsid.length());
    memcpy(saved.password,candidatePassword.c_str(),candidatePassword.length());
    Preferences prefs;
    bool ok=prefs.begin("intercom-wifi",false);
    if(ok) {ok=prefs.putBytes("credentials",&saved,sizeof(saved))==sizeof(saved);prefs.end();}
    candidateResult=ok?"connected":"save_failed";
    if(ok) {configuredSsid=candidateSsid;configuredPassword=candidatePassword;}
    else WiFi.begin(configuredSsid.c_str(),configuredPassword.c_str());
    candidateActive=false;candidatePassword="";
  } else if(millis()-candidateStarted>=35000) {
    candidateResult="failed";candidateActive=false;candidatePassword="";
    WiFi.begin(configuredSsid.c_str(),configuredPassword.c_str());
  }
}
static void handleStationDiscovery() {
  int count=discoveryUdp.parsePacket();
  if(count<=0)return;
  char request[48]={};int length=discoveryUdp.read(request,sizeof(request)-1);
  if(length>0 && String(request)=="DISCOVER_OPEN_INTERCOM_V2") {
    IPAddress remote=discoveryUdp.remoteIP();uint16_t port=discoveryUdp.remotePort();
    discoveryUdp.beginPacket(remote,port);
    discoveryUdp.print("OPEN_INTERCOM_V2 ");discoveryUdp.print(WiFi.macAddress());
    discoveryUdp.endPacket();
  }
}
static void startStationHttp() {
  static bool handlersReady = false;
  if (!handlersReady) {
    wifiSetupServer.on("/", HTTP_GET, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } wifiSetupPage(); });
    wifiSetupServer.on("/save", HTTP_POST, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } wifiSetupSave(); });
    wifiSetupServer.on("/api/info", HTTP_GET, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } apiWifiInfo(); });
    wifiSetupServer.on("/api/scan", HTTP_GET, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } apiWifiScan(); });
    wifiSetupServer.on("/api/profiles", HTTP_GET, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } apiWifiProfiles(); });
    wifiSetupServer.on("/api/profiles", HTTP_POST, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } apiWifiAdditional(); });
    wifiSetupServer.on("/api/connect", HTTP_POST, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } apiWifiConnect(); });
    wifiSetupServer.on("/api/prepare", HTTP_POST, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } apiWifiConnect(true); });
    wifiSetupServer.on("/api/confirm", HTTP_POST, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } apiWifiConfirm(); });
    wifiSetupServer.on("/api/status", HTTP_GET, [] { if (!wifiSetupActive) { wifiSetupServer.send(403, "text/plain", "Einrichtung nicht aktiv"); return; } apiWifiStatus(); });
    wifiSetupServer.onNotFound([] { wifiSetupServer.send(404, "text/plain", "Nicht gefunden"); });
    handlersReady = true;
  }
  static bool started = false;
  if (!started) { wifiSetupServer.begin(); started = true; }
}

static void startWifiSetup() {
  if (wifiSetupActive) { wifiSetupStarted = millis(); return; }
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  if (!WiFi.softAP("OpenIntercom-Setup", "intercomsetup")) {
    Serial.println("[WIFI SETUP] AP start failed"); return;
  }
  wifiSetupToken = String(esp_random(), HEX) + String(esp_random(), HEX);
  startStationHttp();
  wifiSetupActive = true;
  wifiSetupStarted = millis();
  Serial.println("[WIFI SETUP] OpenIntercom-Setup ready: http://192.168.4.1 (10 minutes)");
}

static void handleWifiSetup() {
  wifiSetupServer.handleClient();
  if (!wifiSetupActive) return;
  handleWifiCandidate();
  uint32_t now = millis();
  if (wifiRestartPending && (int32_t)(now - wifiRestartAt) >= 0) ESP.restart();
  if (!wifiRestartPending && !candidateActive && WiFi.status() == WL_CONNECTED &&
      now - wifiSetupStarted >= WIFI_SETUP_DURATION_MS) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    wifiSetupActive = false;
    Serial.println("[WIFI SETUP] Setup ended");
  }
}
