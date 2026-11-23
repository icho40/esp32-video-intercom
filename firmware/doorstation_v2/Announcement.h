#pragma once
// Ansage bei Abwesenheit.
// Satzsammlung: bis zu ANN_SLOTS Saetze fuer alle Parteien, je Satz eine Datei
// /satzN.pcm in LittleFS (64 Byte Kopf mit Abtastrate und Name, danach 16 Bit
// Mono PCM). Der Speicher wird frei aufgeteilt; die Admin-Seite /ansage zeigt
// den Rest. Nur der Admin pflegt die Sammlung (Passwort).
// Auswahl je Klingel (P1 EG, P2 1. Stock, P3 Keller): Satz oder Aus, "sofort
// ansagen, nicht klingeln" und Ende (Unix-Zeit, 0 = bis zum Ausschalten).
// Die Bewohner waehlen am Tablet/Handy (Steuerkanal 5001: ANNSET), ohne Passwort.
// Ablauf beim Klingeln: normal nach der Wartezeit, im Sofort-Modus nach 1 s.
// Der Ruf bleibt annehmbar; Annahme oder Auflegen bricht die Ansage ab. Danach
// gilt der Ruf (nach Ablauf der Wartezeit) als verpasst.
// Benoetigt (im .ino davor definiert): annRequestGen/annDoneGen/annRequestId,
// ringWaitMs, getStateSnapshot, speakerTx, PIN_SPK_SD, OtaUpdate.h, jsonQuote.
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include "DefaultAnnouncements.h"

static constexpr uint8_t ANN_SLOTS = 8;
static constexpr uint32_t ANN_MAX_SECONDS = 20;
static constexpr uint32_t ANN_HEADER = 64;
static constexpr uint32_t ANN_MAGIC = 0x3141494F;  // "OIA1"
static constexpr uint32_t ANN_SPEAKER_RATE = 16000;  // Takt der Sprechverbindung
static constexpr uint32_t ANN_INSTANT_DELAY_MS = 1000;
static constexpr uint32_t ANN_FREE_RESERVE = 24 * 1024;
static constexpr int32_t RING_WAIT_MIN_S = 10, RING_WAIT_MAX_S = 120, RING_WAIT_DEFAULT_S = 30;
static constexpr int32_t ANN_VOLUME_DEFAULT = 80;  // Prozent
static constexpr time_t ANN_TIME_VALID = 1700000000;  // davor: Uhr noch nicht gestellt

struct AnnEntry { uint32_t rate; uint32_t bytes; char name[48]; };  // bytes == 0: Platz frei
struct AnnFloor { uint8_t sel; uint8_t instant; uint32_t until; };  // sel 0 = Aus
struct __attribute__((packed)) AnnFileHeader { uint32_t magic; uint32_t rate; uint32_t reserved[2]; char name[48]; };
static_assert(sizeof(AnnFileHeader) == ANN_HEADER, "Kopf muss 64 Byte haben");

static AnnEntry annLib[ANN_SLOTS + 1];
static AnnFloor annFloor[4];
static portMUX_TYPE annMux = portMUX_INITIALIZER_UNLOCKED;
static std::atomic<int32_t> annVolumePct{ANN_VOLUME_DEFAULT};
static std::atomic<uint8_t> annTestId{0};
static std::atomic<bool> annTestTone{false}, annTestNoSwitch{false};  // USB-Diagnose
static bool annFsReady = false;
static File annUploadFile;
static uint8_t annUploadSlot = 0;
static uint32_t annUploadBytes = 0, annUploadLimit = 0, annUploadRate = 0;
static bool annUploadOk = false;
static String annUploadError, annUploadName;

static const uint32_t ANN_RATES[] = {16000, 22050, 24000, 32000, 44100, 48000};

static String annPath(uint8_t id) { return String("/satz") + id + ".pcm"; }
static const char* ANN_TMP = "/satz.tmp";

static int32_t clampInt(long v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : (int32_t)v; }
static uint32_t annDurationMs(const AnnEntry& e) { return e.rate ? (uint32_t)((uint64_t)e.bytes * 1000 / (e.rate * 2)) : 0; }
static bool annRateOk(uint32_t r) { for (uint32_t x : ANN_RATES) if (x == r) return true; return false; }
static bool annTimeValid() { return time(nullptr) > ANN_TIME_VALID; }

static AnnEntry annEntry(uint8_t id) {
  AnnEntry e = {};
  if (id < 1 || id > ANN_SLOTS) return e;
  portENTER_CRITICAL(&annMux);
  e = annLib[id];
  portEXIT_CRITICAL(&annMux);
  return e;
}

static AnnFloor annFloorGet(uint8_t button) {
  AnnFloor f = {};
  if (button < 1 || button > 3) return f;
  portENTER_CRITICAL(&annMux);
  f = annFloor[button];
  portEXIT_CRITICAL(&annMux);
  return f;
}

static void annReadSlot(uint8_t id) {
  AnnEntry e = {};
  File file = annFsReady ? LittleFS.open(annPath(id), "r") : File();
  if (file) {
    AnnFileHeader h = {};
    if (file.size() > ANN_HEADER && file.read((uint8_t*)&h, sizeof(h)) == sizeof(h) &&
        h.magic == ANN_MAGIC && annRateOk(h.rate)) {
      e.rate = h.rate;
      e.bytes = (file.size() - ANN_HEADER) & ~1u;
      memcpy(e.name, h.name, sizeof(e.name));
      e.name[sizeof(e.name) - 1] = 0;
    }
    file.close();
  }
  portENTER_CRITICAL(&annMux);
  annLib[id] = e;
  portEXIT_CRITICAL(&annMux);
}

static void annLoadLibrary() { for (uint8_t id = 1; id <= ANN_SLOTS; ++id) annReadSlot(id); }

static void annSaveFloors() {
  AnnFloor copy[4];
  portENTER_CRITICAL(&annMux);
  memcpy(copy, annFloor, sizeof(copy));
  portEXIT_CRITICAL(&annMux);
  Preferences prefs;
  if (!prefs.begin("intercom-annc", false)) return;
  for (int b = 1; b <= 3; ++b) {
    char key[8];
    snprintf(key, sizeof(key), "sel%d", b);  prefs.putUChar(key, copy[b].sel);
    snprintf(key, sizeof(key), "inst%d", b); prefs.putUChar(key, copy[b].instant);
    snprintf(key, sizeof(key), "until%d", b); prefs.putULong(key, copy[b].until);
  }
  prefs.end();
}

static void loadAnnouncementSettings() {
  Preferences prefs;
  if (!prefs.begin("intercom-annc", true)) return;
  ringWaitMs = (uint32_t)clampInt(prefs.getInt("wait", RING_WAIT_DEFAULT_S), RING_WAIT_MIN_S, RING_WAIT_MAX_S) * 1000;
  annVolumePct = clampInt(prefs.getInt("vol", ANN_VOLUME_DEFAULT), 10, 100);
  for (int b = 1; b <= 3; ++b) {
    char key[8];
    AnnFloor f = {};
    snprintf(key, sizeof(key), "sel%d", b);  f.sel = prefs.getUChar(key, 0);
    snprintf(key, sizeof(key), "inst%d", b); f.instant = prefs.getUChar(key, 0) ? 1 : 0;
    snprintf(key, sizeof(key), "until%d", b); f.until = prefs.getULong(key, 0);
    if (f.sel > ANN_SLOTS) f = {};
    annFloor[b] = f;
  }
  prefs.end();
}

// Auswahl einer Klingel setzen (vom Tablet/Handy oder der Admin-Seite).
static bool annSetFloor(uint8_t button, uint8_t sel, bool instant, uint32_t until) {
  if (button < 1 || button > 3 || sel > ANN_SLOTS) return false;
  if (sel && !annEntry(sel).bytes) return false;
  if (!sel) { instant = false; until = 0; }
  portENTER_CRITICAL(&annMux);
  annFloor[button] = {sel, (uint8_t)(instant ? 1 : 0), until};
  portEXIT_CRITICAL(&annMux);
  annSaveFloors();
  annVersion++;
  Serial.printf("[ANSAGE] P%u: Satz %u%s, Ende %lu\n", button, sel, instant ? " sofort" : "", (unsigned long)until);
  return true;
}

// Aktive Ansage einer Klingel (Satz vorhanden, Zeit nicht abgelaufen).
static bool annActive(uint8_t button, AnnFloor& floor, AnnEntry& entry) {
  floor = annFloorGet(button);
  if (!floor.sel) return false;
  if (floor.until && annTimeValid() && (uint32_t)time(nullptr) >= floor.until) return false;
  entry = annEntry(floor.sel);
  return entry.bytes > 0;
}

// Einmal pro Sekunde aus loop(): abgelaufene Auswahl ausschalten.
static void annCheckExpiry() {
  if (!annTimeValid()) return;
  uint32_t now = (uint32_t)time(nullptr);
  for (uint8_t b = 1; b <= 3; ++b) {
    AnnFloor f = annFloorGet(b);
    if (f.sel && f.until && now >= f.until) {
      Serial.printf("[ANSAGE] P%u: Zeit abgelaufen, Ansage aus\n", b);
      annSetFloor(b, 0, false, 0);
    }
  }
}

// Uhrzeit vom Tablet/Handy, solange die Station keine Internetzeit hat.
static void annSetClockFromClient(uint32_t epoch) {
  if (annTimeValid() || epoch < ANN_TIME_VALID) return;
  struct timeval tv = {(time_t)epoch, 0};
  settimeofday(&tv, nullptr);
  Serial.printf("[ANSAGE] Uhrzeit vom Tablet uebernommen: %lu\n", (unsigned long)epoch);
}

static void annRemoveSlot(uint8_t id) {
  if (id < 1 || id > ANN_SLOTS) return;
  portENTER_CRITICAL(&annMux);
  annLib[id] = {};
  portEXIT_CRITICAL(&annMux);
  LittleFS.remove(annPath(id));
  for (uint8_t b = 1; b <= 3; ++b) if (annFloorGet(b).sel == id) annSetFloor(b, 0, false, 0);
  annVersion++;
}

static uint8_t annFreeSlot() {
  for (uint8_t id = 1; id <= ANN_SLOTS; ++id) if (!annEntry(id).bytes) return id;
  return 0;
}

static uint32_t annFreeBytes() {
  if (!annFsReady) return 0;
  size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  return total > used + ANN_FREE_RESERVE ? (uint32_t)(total - used - ANN_FREE_RESERVE) : 0;
}

static AnnFileHeader annMakeHeader(uint32_t rate, const String& name) {
  AnnFileHeader h = {};
  h.magic = ANN_MAGIC;
  h.rate = rate;
  strncpy(h.name, name.c_str(), sizeof(h.name) - 1);
  return h;
}

// Erststart: die eingebauten Saetze in die Sammlung schreiben (nur einmal,
// danach entscheidet der Admin). Alte Ansagen je Klingel entfernen.
static void annSeedDefaults() {
  for (int f = 1; f <= 3; ++f) LittleFS.remove(String("/ansage") + f + ".pcm");
  LittleFS.remove(ANN_TMP);
  Preferences prefs;
  if (!prefs.begin("intercom-annc", false)) return;
  if (!prefs.getBool("seeded", false)) {
    for (size_t i = 0; i < sizeof(ANN_DEFAULTS) / sizeof(ANN_DEFAULTS[0]); ++i) {
      uint8_t id = annFreeSlot();
      if (!id) break;
      File file = LittleFS.open(annPath(id), "w");
      if (!file) break;
      AnnFileHeader h = annMakeHeader(ANN_DEFAULTS[i].rate, ANN_DEFAULTS[i].name);
      bool ok = file.write((const uint8_t*)&h, sizeof(h)) == sizeof(h) &&
                file.write(ANN_DEFAULTS[i].data, ANN_DEFAULTS[i].bytes) == ANN_DEFAULTS[i].bytes;
      file.close();
      if (!ok) { LittleFS.remove(annPath(id)); break; }
      annReadSlot(id);
      Serial.printf("[ANSAGE] Satz %u eingerichtet: %s\n", id, ANN_DEFAULTS[i].name);
    }
    prefs.putBool("seeded", true);
  }
  prefs.end();
}

static void annSpeakerRate(uint32_t rate) {
  i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(rate);
  i2s_channel_disable(speakerTx);
  i2s_channel_reconfig_std_clock(speakerTx, &clk);
  i2s_channel_enable(speakerTx);
}

// Laeuft im Lautsprecher-Task (einziger Schreiber auf speakerTx).
// gen != 0: Ruf-Ansage, Abbruch sobald sich der Zustand aendert.
// gen == 0: Probe von der Webseite, Abbruch sobald es klingelt.
static void playAnnouncement(uint8_t id, uint32_t gen, bool noSwitch = false) {
  if (!annFsReady || speakerTx == nullptr || id < 1 || id > ANN_SLOTS) return;
  AnnEntry e = annEntry(id);
  if (!e.bytes) return;
  File file = LittleFS.open(annPath(id), "r");
  if (!file) return;
  file.seek(ANN_HEADER);
  static uint8_t mono[512];  // statisch: Lautsprecher-Task hat wenig Stack
  static int16_t stereo[512];
  int32_t vol = annVolumePct;
  bool otherRate = e.rate != ANN_SPEAKER_RATE && !noSwitch;
  if (otherRate) annSpeakerRate(e.rate);  // Verstaerker ist hier aus
  digitalWrite(PIN_SPK_SD, HIGH);
  Serial.printf("[ANSAGE] Satz %u start (%lu ms, %lu Hz)%s\n", id, (unsigned long)annDurationMs(e),
                (unsigned long)e.rate, gen ? "" : " Probe");
  bool aborted = false;
  while (true) {
    StateSnapshot s = getStateSnapshot();
    if (gen ? s.generation != gen : s.state != IntercomState::IDLE) { aborted = true; break; }
    int n = file.read(mono, sizeof(mono));
    if (n <= 1) break;
    int frames = n / 2;
    for (int i = 0; i < frames; ++i) {
      int16_t v = (int16_t)((uint16_t)mono[i * 2] | ((uint16_t)mono[i * 2 + 1] << 8));
      int16_t out = (int16_t)((int32_t)v * vol / 100);
      stereo[i * 2] = out;
      stereo[i * 2 + 1] = out;
    }
    size_t written = 0;
    i2s_channel_write(speakerTx, stereo, frames * 4, &written, pdMS_TO_TICKS(200));
  }
  file.close();
  // Kurze Stille, damit das Ende nicht abgeschnitten wird.
  memset(stereo, 0, sizeof(stereo));
  size_t written = 0;
  int pad = aborted ? 1 : (int)(e.rate / 256 / 8) + 1;  // ca. 120 ms
  for (int i = 0; i < pad; ++i) i2s_channel_write(speakerTx, stereo, sizeof(stereo), &written, pdMS_TO_TICKS(200));
  digitalWrite(PIN_SPK_SD, LOW);
  if (otherRate) {
    annSpeakerRate(ANN_SPEAKER_RATE);
    for (int i = 0; i < 2; ++i) i2s_channel_write(speakerTx, stereo, sizeof(stereo), &written, pdMS_TO_TICKS(100));
  }
  Serial.printf("[ANSAGE] Satz %u %s\n", id, aborted ? "abgebrochen" : "fertig");
}

// Zeilen fuer Tablets/Handys (Steuerkanal 5001), bei Verbindung und Aenderung:
//   ANNLIB BEGIN / ANNLIB <id> <ms> <Name> / ANNLIB END
//   ANN P<n> <Satz> <sofort 0|1> <Ende Unix-Zeit, 0 = unbegrenzt>
static String annControlLines() {
  String out = "ANNLIB BEGIN\n";
  for (uint8_t id = 1; id <= ANN_SLOTS; ++id) {
    AnnEntry e = annEntry(id);
    if (!e.bytes) continue;
    out += "ANNLIB " + String(id) + " " + String(annDurationMs(e)) + " " + String(e.name) + "\n";
  }
  out += "ANNLIB END\n";
  for (uint8_t b = 1; b <= 3; ++b) {
    AnnFloor f = annFloorGet(b);
    out += "ANN P" + String(b) + " " + String(f.sel) + " " + String(f.instant) + " " + String(f.until) + "\n";
  }
  return out;
}

// Testton 440 Hz, 1 s, 16 kHz (USB-Diagnose, im Lautsprecher-Task).
static void annPlayTone() {
  if (speakerTx == nullptr) return;
  static int16_t samples[512];
  digitalWrite(PIN_SPK_SD, HIGH);
  for (int offset = 0; offset < 16000; offset += 256) {
    for (int i = 0; i < 256; ++i) {
      int16_t v = (int16_t)(sinf(2.0f * PI * 440.0f * (offset + i) / 16000.0f) * 8000.0f);
      samples[i * 2] = v; samples[i * 2 + 1] = v;
    }
    size_t written = 0;
    i2s_channel_write(speakerTx, samples, sizeof(samples), &written, pdMS_TO_TICKS(200));
  }
  memset(samples, 0, sizeof(samples));
  size_t written = 0;
  i2s_channel_write(speakerTx, samples, sizeof(samples), &written, pdMS_TO_TICKS(200));
  digitalWrite(PIN_SPK_SD, LOW);
  Serial.println("[ANSAGE] Testton fertig");
}

// USB-Befehle (Serial, 115200): ANSAGE_LIST, ANSAGE_PLAY <Satz>,
// ANSAGE_PLAY16 <Satz> (ohne Umschalten, klingt zu langsam), SPEAKER_TONE
static bool annSerialCommand(const String& line) {
  if (line == "ANSAGE_LIST") {
    for (uint8_t id = 1; id <= ANN_SLOTS; ++id) {
      AnnEntry e = annEntry(id);
      if (e.bytes) Serial.printf("ANSAGE_SATZ %u %lu ms %lu Hz %s\n", id, (unsigned long)annDurationMs(e), (unsigned long)e.rate, e.name);
    }
    for (uint8_t b = 1; b <= 3; ++b) {
      AnnFloor f = annFloorGet(b);
      Serial.printf("ANSAGE_P%u satz=%u sofort=%u bis=%lu\n", b, f.sel, f.instant, (unsigned long)f.until);
    }
    Serial.printf("ANSAGE_FREI %lu Bytes, Uhr %s\n", (unsigned long)annFreeBytes(), annTimeValid() ? "gestellt" : "nicht gestellt");
    return true;
  }
  if (line == "SPEAKER_TONE") {
    annTestTone = true;
    Serial.println("SPEAKER_TONE_OK");
    return true;
  }
  if (line.startsWith("ANSAGE_PLAY16 ")) {
    uint8_t id = line.substring(14).toInt();
    bool ok = annEntry(id).bytes && getIntercomState() == IntercomState::IDLE;
    if (ok) { annTestNoSwitch = true; annTestId = id; }
    Serial.println(ok ? "ANSAGE_PLAY_OK" : "ANSAGE_PLAY_ERROR");
    return true;
  }
  if (line.startsWith("ANSAGE_PLAY ")) {
    uint8_t id = line.substring(12).toInt();
    bool ok = annEntry(id).bytes && getIntercomState() == IntercomState::IDLE;
    if (ok) annTestId = id;
    Serial.println(ok ? "ANSAGE_PLAY_OK" : "ANSAGE_PLAY_ERROR");
    return true;
  }
  return false;
}

static bool annTokenOk() { return wifiSetupServer.arg("token") == otaToken; }

static void annSendText(int code, const char* text) {
  wifiSetupServer.send(code, "text/plain; charset=utf-8", text);
}

static String annLibraryJson() {
  String j = "[";
  for (uint8_t id = 1; id <= ANN_SLOTS; ++id) {
    AnnEntry e = annEntry(id);
    if (!e.bytes) continue;
    if (j.length() > 1) j += ",";
    j += "{\"id\":" + String(id) + ",\"n\":" + jsonQuote(e.name) + ",\"ms\":" + String(annDurationMs(e)) +
         ",\"r\":" + String(e.rate) + "}";
  }
  return j + "]";
}

static String annFloorsJson() {
  String j = "[null";
  for (uint8_t b = 1; b <= 3; ++b) {
    AnnFloor f = annFloorGet(b);
    j += ",{\"sel\":" + String(f.sel) + ",\"inst\":" + String(f.instant) + ",\"until\":" + String(f.until) + "}";
  }
  return j + "]";
}

static void annSendWav(uint8_t id) {
  AnnEntry e = annEntry(id);
  File file = (annFsReady && e.bytes) ? LittleFS.open(annPath(id), "r") : File();
  if (!file) { annSendText(404, "Kein Satz."); return; }
  file.seek(ANN_HEADER);
  uint8_t h[44];
  auto u32 = [&](int o, uint32_t v) { h[o] = v; h[o + 1] = v >> 8; h[o + 2] = v >> 16; h[o + 3] = v >> 24; };
  auto u16 = [&](int o, uint16_t v) { h[o] = v; h[o + 1] = v >> 8; };
  memcpy(h, "RIFF", 4); u32(4, 36 + e.bytes); memcpy(h + 8, "WAVEfmt ", 8);
  u32(16, 16); u16(20, 1); u16(22, 1); u32(24, e.rate); u32(28, e.rate * 2); u16(32, 2); u16(34, 16);
  memcpy(h + 36, "data", 4); u32(40, e.bytes);
  wifiSetupServer.sendHeader("Cache-Control", "no-store");
  wifiSetupServer.setContentLength(44 + e.bytes);
  wifiSetupServer.send(200, "audio/wav", "");
  // Grosse Bloecke ohne Nagle-Verzoegerung; bricht ab, wenn der Abrufer weg ist,
  // damit der Webserver (loop) nicht an einer toten Verbindung haengt.
  NetworkClient client = wifiSetupServer.client();
  client.setNoDelay(true);
  client.write(h, sizeof(h));
  static uint8_t* buf = nullptr;
  constexpr size_t BUF = 16 * 1024;
  if (!buf) buf = (uint8_t*)ps_malloc(BUF);
  uint32_t left = e.bytes, started = millis();
  while (left && buf && client.connected() && millis() - started < 30000) {
    int n = file.read(buf, left < BUF ? left : BUF);
    if (n <= 0) break;
    size_t sent = client.write(buf, n);
    if (sent != (size_t)n) break;
    left -= n;
  }
  if (left) Serial.printf("[ANSAGE] Satz %u: Abruf abgebrochen (%lu Bytes offen)\n", id, (unsigned long)left);
  file.close();
}

static void startAnnouncements() {
  annFsReady = LittleFS.begin(true, "/littlefs", 4, "spiffs");
  if (!annFsReady) Serial.println("[ANSAGE] LittleFS nicht verfuegbar");
  loadAnnouncementSettings();
  if (annFsReady) { annLoadLibrary(); annSeedDefaults(); }
  // Uhrzeit aus dem Internet (UTC); ohne Internet stellen Tablets/Handys die Uhr.
  configTime(0, 0, "pool.ntp.org", "time.google.com", "de.pool.ntp.org");
  for (uint8_t b = 1; b <= 3; ++b) {
    uint8_t sel = annFloorGet(b).sel;
    if (sel && !annEntry(sel).bytes) annSetFloor(b, 0, false, 0);
  }
  int count = 0;
  for (uint8_t id = 1; id <= ANN_SLOTS; ++id) if (annEntry(id).bytes) count++;
  Serial.printf("[ANSAGE] Wartezeit %lu s, Lautstaerke %ld %%, %d Saetze, frei %lu Bytes\n",
                (unsigned long)(ringWaitMs / 1000), (long)annVolumePct.load(), count, (unsigned long)annFreeBytes());

  // Satz zum Probehoeren fuer Tablets/Handys (nur lokales Netz, ohne Passwort).
  wifiSetupServer.on("/api/ansage.wav", HTTP_GET, [] {
    if (!otaLocalClient()) { annSendText(403, "Nur im lokalen Netz"); return; }
    annSendWav(wifiSetupServer.arg("id").toInt());
  });

  wifiSetupServer.on("/ansage", HTTP_GET, [] {
    if (!otaAuthorized()) { otaReject(); return; }
    String html = R"HTML(<!doctype html><html lang="de"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Ansagen</title>
<style>body{font-family:system-ui,sans-serif;background:#111318;color:#e8ecf2;max-width:680px;margin:24px auto;padding:0 16px}
a{color:#7edbc1}h2{margin:28px 0 6px;font-size:19px}.box{background:#1b1f27;border-radius:10px;padding:12px 14px;margin:10px 0}
.st{color:#b5c6d2;margin:4px 0 8px}button,input[type=file],select{font-size:15px;margin:4px 6px 4px 0;padding:8px 12px}
label{display:block;margin:14px 0 4px;font-weight:600}input[type=number],input[type=text]{font-size:16px;padding:6px}
input[type=text]{width:100%;box-sizing:border-box}input[type=number]{width:90px}
input[type=range]{width:100%}.note{color:#b5c6d2;font-size:14px}#msg{color:#7edbc1;min-height:1.3em}
table{width:100%;border-collapse:collapse}td{padding:6px 4px;border-bottom:1px solid #2a303b}b{color:#fff}</style>
<h1>Türstation · Ansagen</h1><p><a href="/audio">Lautstärke</a> · <a href="/diagnose">Diagnose</a></p>
<p class="note">Alle Parteien wählen am Tablet oder Handy über „Ansage bei Abwesenheit“ einen Satz aus dieser
Sammlung. Sätze bitte neutral halten, ohne Namen und ohne Etage. Nimmt nach der Wartezeit niemand ab
(im Sofort-Modus nach 1 Sekunde), spielt die Türstation den gewählten Satz ab.</p>
<p id="msg" role="status">%OK%</p>
<h2>Auswahl der Parteien</h2><div class="box"><table id="floors"></table></div>
<h2>Sätze</h2><div id="lib"></div>
<h2>Neuer Satz</h2><div class="box">
<div class="st">Frei: <span id="free"></span></div>
<label>Name (so erscheint er auf Tablet und Handy)</label><input type="text" id="name" maxlength="40">
<label>Audiodatei (WAV, MP3, M4A …, höchstens 20 Sekunden)</label><input type="file" accept="audio/*,.wav,.mp3,.m4a,.ogg" id="file">
<label>Qualität</label><select id="rate"><option value="44100" selected>CD-Qualität (44,1 kHz)</option>
<option value="32000">32 kHz</option><option value="16000">Sprechverbindung (16 kHz)</option></select><br>
<button onclick="up()">Hochladen</button></div>
<h2>Einstellungen</h2><div class="box"><form method="post" action="/ansage/settings">
<label>Wartezeit bis zur Ansage (Sekunden, 10–120)</label><input type="number" name="wait" min="10" max="120" value="%WAIT%">
<label>Lautstärke der Ansage</label><input type="range" name="vol" min="10" max="100" step="5" value="%VOL%" oninput="v.value=this.value+' %'"><output id="v">%VOL% %</output>
<input type="hidden" name="token" value="%TOKEN%"><br><button type="submit">Speichern</button></form></div>
<script>
const T='%TOKEN%',L=%LIB%,F=%FLOORS%,FREE=%FREE%,N=['','Erdgeschoß (P1)','1. Stock (P2)','Keller (P3)'];
const msg=t=>document.getElementById('msg').textContent=t;
const esc=s=>s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const sec=ms=>(ms/1000).toFixed(1).replace('.',',')+' s';
function draw(){let h='';for(let f=1;f<=3;f++){const s=F[f],e=L.find(x=>x.id==s.sel);
h+='<tr><td>'+N[f]+'</td><td><b>'+(e?esc(e.n):'Aus')+'</b>'+(e&&s.inst?' · sofort, ohne Klingeln':'')+
(e&&s.until?' · bis '+new Date(s.until*1000).toLocaleString('de-DE',{dateStyle:'short',timeStyle:'short'}):'')+
'</td><td>'+(e?'<button onclick="off('+f+')">Ausschalten</button>':'')+'</td></tr>'}
document.getElementById('floors').innerHTML=h;h='';
for(const e of L)h+='<div class="box"><b>'+esc(e.n)+'</b><div class="st">'+sec(e.ms)+' · '+(e.r/1000).toString().replace('.',',')+' kHz</div>'+
'<button onclick="new Audio(\'/api/ansage.wav?id='+e.id+'&t='+Date.now()+'\').play()">Hier anhören</button>'+
'<button onclick="post(\'play\','+e.id+')">An der Tür abspielen</button>'+
'<button onclick="if(confirm(\'Satz löschen? Parteien, die ihn gewählt haben, stehen danach auf Aus.\'))post(\'delete\','+e.id+')">Löschen</button></div>';
document.getElementById('lib').innerHTML=h||'<div class="box st">Noch keine Sätze.</div>';
document.getElementById('free').textContent=(FREE/1048576).toFixed(2).replace('.',',')+' MB · etwa '+sec(FREE/88.2)+' in CD-Qualität'}
async function post(a,id){const r=await fetch('/ansage/'+a+'?id='+id+'&token='+T,{method:'POST'});const t=await r.text();if(a=='delete'&&r.ok)location.href='/ansage?ok=geloescht';else msg(t)}
async function off(f){const r=await fetch('/ansage/floor?floor='+f+'&sel=0&token='+T,{method:'POST'});if(r.ok)location.href='/ansage?ok=aus';else msg(await r.text())}
async function up(){const i=document.getElementById('file'),name=document.getElementById('name').value.trim(),rate=+document.getElementById('rate').value;
if(!name){msg('Bitte einen Namen eingeben.');return}if(!i.files.length){msg('Bitte zuerst eine Datei auswählen.');return}
try{msg('Wird umgewandelt …');const ac=new (window.AudioContext||window.webkitAudioContext)();const src=await ac.decodeAudioData(await i.files[0].arrayBuffer());ac.close&&ac.close();
if(src.duration>20.05){msg('Die Datei ist länger als 20 Sekunden. Bitte kürzen.');return}
const len=Math.ceil(src.duration*rate);if(len*2>FREE){msg('Nicht genug Platz. Bitte einen Satz löschen oder eine geringere Qualität wählen.');return}
const oc=new OfflineAudioContext(1,len,rate);const b=oc.createBufferSource();b.buffer=src;b.connect(oc.destination);b.start();
const out=(await oc.startRendering()).getChannelData(0);let peak=0;for(const x of out)peak=Math.max(peak,Math.abs(x));const g=peak>0?0.9/peak:1;
const pcm=new Int16Array(len);for(let k=0;k<len;k++)pcm[k]=Math.max(-32768,Math.min(32767,Math.round(out[k]*g*32767)));
const fd=new FormData();fd.append('pcm',new Blob([pcm.buffer]),'satz.pcm');msg('Wird hochgeladen …');
const r=await fetch('/ansage/upload?rate='+rate+'&name='+encodeURIComponent(name)+'&token='+T,{method:'POST',body:fd});const t=await r.text();
if(r.ok)location.href='/ansage?ok=gespeichert';else msg(t)}catch(e){msg('Datei konnte nicht gelesen werden: '+e)}}
draw();
</script></html>)HTML";
    String ok = wifiSetupServer.arg("ok");
    html.replace("%OK%", ok == "gespeichert" ? "Satz gespeichert." : ok == "geloescht" ? "Satz gelöscht." :
                         ok == "aus" ? "Ansage ausgeschaltet." :
                         ok == "einstellungen" ? "Einstellungen gespeichert." : "");
    html.replace("%WAIT%", String(ringWaitMs / 1000));
    html.replace("%VOL%", String(annVolumePct.load()));
    html.replace("%LIB%", annLibraryJson());
    html.replace("%FLOORS%", annFloorsJson());
    html.replace("%FREE%", String(annFreeBytes()));
    html.replace("%TOKEN%", otaToken);
    wifiSetupServer.sendHeader("Cache-Control", "no-store");
    wifiSetupServer.setContentLength(html.length());
    wifiSetupServer.send(200, "text/html; charset=utf-8", "");
    for (size_t offset = 0; offset < html.length(); offset += 1024)
      wifiSetupServer.sendContent(html.substring(offset, offset + 1024));
  });

  wifiSetupServer.on("/ansage/settings", HTTP_POST, [] {
    if (!otaAuthorized()) { otaReject(); return; }
    if (!annTokenOk()) { annSendText(400, "Ungültige Anfrage. Seite neu laden."); return; }
    int32_t wait = clampInt(wifiSetupServer.arg("wait").toInt(), RING_WAIT_MIN_S, RING_WAIT_MAX_S);
    int32_t vol = clampInt(wifiSetupServer.arg("vol").toInt(), 10, 100);
    ringWaitMs = (uint32_t)wait * 1000;
    annVolumePct = vol;
    Preferences prefs;
    if (prefs.begin("intercom-annc", false)) { prefs.putInt("wait", wait); prefs.putInt("vol", vol); prefs.end(); }
    Serial.printf("[ANSAGE] Wartezeit %ld s, Lautstaerke %ld %%\n", (long)wait, (long)vol);
    wifiSetupServer.sendHeader("Location", "/ansage?ok=einstellungen");
    wifiSetupServer.send(303, "text/plain", "");
  });

  wifiSetupServer.on("/ansage/floor", HTTP_POST, [] {
    if (!otaAuthorized()) { otaReject(); return; }
    uint8_t f = wifiSetupServer.arg("floor").toInt();
    if (!annTokenOk() || !annSetFloor(f, wifiSetupServer.arg("sel").toInt(), false, 0)) {
      annSendText(400, "Ungültige Anfrage. Seite neu laden."); return;
    }
    annSendText(200, "Gespeichert.");
  });

  wifiSetupServer.on("/ansage/play", HTTP_POST, [] {
    if (!otaAuthorized()) { otaReject(); return; }
    uint8_t id = wifiSetupServer.arg("id").toInt();
    if (!annTokenOk() || !annEntry(id).bytes) { annSendText(400, "Ungültige Anfrage. Seite neu laden."); return; }
    if (getIntercomState() != IntercomState::IDLE) { annSendText(409, "Gerade klingelt es oder es läuft ein Gespräch."); return; }
    annTestId = id;
    annSendText(200, "Satz wird an der Tür abgespielt.");
  });

  wifiSetupServer.on("/ansage/delete", HTTP_POST, [] {
    if (!otaAuthorized()) { otaReject(); return; }
    uint8_t id = wifiSetupServer.arg("id").toInt();
    if (!annTokenOk() || !annFsReady || !annEntry(id).bytes) { annSendText(400, "Ungültige Anfrage. Seite neu laden."); return; }
    annRemoveSlot(id);
    Serial.printf("[ANSAGE] Satz %u geloescht\n", id);
    annSendText(200, "Satz gelöscht.");
  });

  wifiSetupServer.on("/ansage/upload", HTTP_POST, [] {
    if (!otaAuthorized()) { otaReject(); return; }
    if (annUploadOk) annSendText(200, "Satz gespeichert.");
    else annSendText(400, annUploadError.length() ? annUploadError.c_str() : "Hochladen fehlgeschlagen.");
  }, [] {
    HTTPUpload& upload = wifiSetupServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      annUploadOk = false;
      annUploadError = "";
      annUploadBytes = 0;
      if (annUploadFile) annUploadFile.close();
      annUploadSlot = 0;
      annUploadRate = wifiSetupServer.arg("rate").toInt();
      annUploadName = wifiSetupServer.arg("name");
      annUploadName.replace("\n", " "); annUploadName.replace("\r", " "); annUploadName.trim();
      if (annUploadName.length() > 47) annUploadName = annUploadName.substring(0, 47);
      if (!otaAuthorized() || !annTokenOk() || !annRateOk(annUploadRate) || !annUploadName.length()) {
        annUploadError = "Ungültige Anfrage. Seite neu laden."; return;
      }
      if (!annFsReady) { annUploadError = "Speicher der Station nicht verfügbar."; return; }
      annUploadSlot = annFreeSlot();
      if (!annUploadSlot) { annUploadError = "Höchstens 8 Sätze. Bitte zuerst einen löschen."; return; }
      uint32_t free = annFreeBytes();
      uint32_t maxBytes = ANN_MAX_SECONDS * annUploadRate * 2 + 4096;
      annUploadLimit = free > ANN_HEADER ? free - ANN_HEADER : 0;
      if (annUploadLimit > maxBytes) annUploadLimit = maxBytes;
      annUploadFile = LittleFS.open(ANN_TMP, "w");
      AnnFileHeader h = annMakeHeader(annUploadRate, annUploadName);
      if (!annUploadFile || annUploadFile.write((const uint8_t*)&h, sizeof(h)) != sizeof(h)) {
        annUploadError = "Datei konnte nicht angelegt werden."; annUploadSlot = 0;
        if (annUploadFile) { annUploadFile.close(); LittleFS.remove(ANN_TMP); }
      }
    } else if (upload.status == UPLOAD_FILE_WRITE && annUploadSlot && annUploadFile) {
      if (annUploadBytes + upload.currentSize > annUploadLimit) {
        annUploadError = "Nicht genug Platz oder länger als 20 Sekunden.";
      } else if (annUploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
        annUploadError = "Speicher voll.";
      }
      annUploadBytes += upload.currentSize;
      if (annUploadError.length()) { annUploadFile.close(); LittleFS.remove(ANN_TMP); annUploadSlot = 0; }
    } else if (upload.status == UPLOAD_FILE_END && annUploadSlot && annUploadFile) {
      annUploadFile.close();
      if (annUploadBytes < annUploadRate / 5) {  // unter 0,1 s
        annUploadError = "Die Datei enthält keinen Ton.";
        LittleFS.remove(ANN_TMP);
      } else {
        LittleFS.remove(annPath(annUploadSlot));
        annUploadOk = LittleFS.rename(ANN_TMP, annPath(annUploadSlot));
        if (!annUploadOk) annUploadError = "Datei konnte nicht gespeichert werden.";
        annReadSlot(annUploadSlot);
        annVersion++;
        Serial.printf("[ANSAGE] Satz %u gespeichert: %s, %lu Bytes, %lu Hz ok=%d\n", annUploadSlot, annUploadName.c_str(),
                      (unsigned long)annUploadBytes, (unsigned long)annUploadRate, annUploadOk);
      }
      annUploadSlot = 0;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      if (annUploadFile) annUploadFile.close();
      LittleFS.remove(ANN_TMP);
      annUploadSlot = 0;
      annUploadError = "Hochladen abgebrochen.";
    }
  });
}
