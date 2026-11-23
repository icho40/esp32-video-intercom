/*
  Waveshare Doorstation Server
  - UART JPEG ingest vom XIAO
  - /stream MJPEG
  - /snapshot
  - LittleFS: /idle.html, /p1/, /p2/, /p3/
  - Taster P1/P2/P3
  - /api/state
  - /api/call/ack
*/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include "driver/uart.h"
#include <memory>
#include <algorithm>

#include "secrets.h"

// ================= CONFIG =================
#define USE_MDNS 1
#define MDNS_NAME "tuer"

#define UART_PORT UART_NUM_1
#define PIN_UART_RX 18
#define UART_BAUD 1000000

#define PIN_BTN_P1 4
#define PIN_BTN_P2 5
#define PIN_BTN_P3 6

#define JPEG_MAX (120 * 1024)

static const uint32_t RING_TIMEOUT_MS = 30 * 1000;
static const uint32_t BTN_DEBOUNCE_MS = 120;

// ================= GLOBALS =================
AsyncWebServer server(80);

enum CallState : uint8_t {
  CS_IDLE = 0,
  CS_RING = 1
};

volatile CallState stateP1 = CS_IDLE;
volatile CallState stateP2 = CS_IDLE;
volatile CallState stateP3 = CS_IDLE;

uint32_t ringSinceP1 = 0;
uint32_t ringSinceP2 = 0;
uint32_t ringSinceP3 = 0;

static uint8_t* gJpeg = nullptr;
static size_t gLen = 0;
static uint32_t gFrames = 0;
static uint32_t gBytes = 0;
static uint32_t gAudioPackets = 0;
static uint32_t gAudioBytes = 0;// ================= AUDIO RING BUFFER =================
#define AUDIO_RING_SIZE (64 * 1024)

static uint8_t* gAudioRing = nullptr;
static size_t gAudioWrite = 0;
static size_t gAudioRead = 0;
static size_t gAudioUsed = 0;

static portMUX_TYPE audioMux = portMUX_INITIALIZER_UNLOCKED;


static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static uint8_t* encBuf = nullptr;
static uint8_t* decBuf = nullptr;
static size_t pktLen = 0;

typedef struct __attribute__((packed)) {
  uint8_t type;
  uint16_t seq;
  uint32_t ts_ms;
  uint32_t payload_len;
} pkt_hdr_t;

// ================= COBS =================
static size_t cobs_decode(const uint8_t* in, size_t len, uint8_t* out) {
  const uint8_t* end = in + len;
  size_t o = 0;

  while (in < end) {
    uint8_t code = *in++;
    if (code == 0) return 0;

    for (uint8_t i = 1; i < code; i++) {
      if (in >= end) return 0;
      out[o++] = *in++;
    }

    if (code < 0xFF && in < end) {
      out[o++] = 0x00;
    }
  }

  return o;
}

static void setFrame(const uint8_t* src, size_t len) {
  taskENTER_CRITICAL(&mux);

  if (gJpeg) {
    free(gJpeg);
    gJpeg = nullptr;
    gLen = 0;
  }

  gJpeg = (uint8_t*)ps_malloc(len);
  if (gJpeg) {
    memcpy(gJpeg, src, len);
    gLen = len;
  }

  taskEXIT_CRITICAL(&mux);
}
static size_t audioRingRead(uint8_t* dst, size_t maxLen) {
  if (!gAudioRing || !dst || maxLen == 0) return 0;

  taskENTER_CRITICAL(&audioMux);

  size_t len = std::min(maxLen, gAudioUsed);

  for (size_t i = 0; i < len; i++) {
    dst[i] = gAudioRing[gAudioRead];
    gAudioRead = (gAudioRead + 1) % AUDIO_RING_SIZE;
  }

  gAudioUsed -= len;

  taskEXIT_CRITICAL(&audioMux);

  return len;
}

static void audioRingFlush() {
  taskENTER_CRITICAL(&audioMux);

  gAudioRead = gAudioWrite;
  gAudioUsed = 0;

  taskEXIT_CRITICAL(&audioMux);
}

static void audioRingWrite(const uint8_t* data, size_t len) {
  if (!gAudioRing || len == 0) return;

  taskENTER_CRITICAL(&audioMux);

  for (size_t i = 0; i < len; i++) {

    // Puffer voll: ältestes Byte verwerfen
    if (gAudioUsed >= AUDIO_RING_SIZE) {
      gAudioRead = (gAudioRead + 1) % AUDIO_RING_SIZE;
      gAudioUsed--;
    }

    gAudioRing[gAudioWrite] = data[i];
    gAudioWrite = (gAudioWrite + 1) % AUDIO_RING_SIZE;
    gAudioUsed++;
  }

  taskEXIT_CRITICAL(&audioMux);
}


// ================= WAV AUDIO STREAM =================
class WAVResponse : public AsyncAbstractResponse {
public:
  WAVResponse()
    : _headerPos(0),
      _started(false),
      _nextSendUs(0) {

    _code = 200;
    _contentType = "audio/wav";
    _sendContentLength = false;

    buildHeader();
  }

  bool _sourceValid() const override {
    return true;
  }

  size_t _fillBuffer(uint8_t* buf, size_t maxLen) override {

    // ---------- WAV-Header ----------
    if (_headerPos < sizeof(_header)) {
      size_t remaining = sizeof(_header) - _headerPos;
      size_t n = std::min(remaining, maxLen);

      memcpy(buf, _header + _headerPos, n);
      _headerPos += n;

      return n;
    }


    // ---------- Startpuffer ----------
    //
    // Erst ungefähr 200 ms Audio sammeln.
    // 16000 Samples/s * 2 Byte * 0,2 s = 6400 Byte.
    //
    if (!_started) {
      audioRingFlush();
      while (true) {
        size_t used;

        taskENTER_CRITICAL(&audioMux);
        used = gAudioUsed;
        taskEXIT_CRITICAL(&audioMux);

        if (used >= 6400) {
          _started = true;

          // Ab jetzt alle 16 ms ein 512-Byte-Paket ausgeben.
          _nextSendUs = micros();

          Serial.printf(
            "[WAV] start buffer=%u bytes\n",
            (unsigned)used
          );

          break;
        }

        delay(1);
      }
    }


    // ---------- Ausgabe takten ----------
    //
    // 512 Byte =
    // 256 Samples =
    // 16 ms bei 16000 Samples/s.
    //
    uint32_t nowUs = micros();

    if ((int32_t)(nowUs - _nextSendUs) < 0) {
      uint32_t waitUs = _nextSendUs - nowUs;

      if (waitUs >= 1000) {
        delay(waitUs / 1000);
      }

      while ((int32_t)(micros() - _nextSendUs) < 0) {
        delayMicroseconds(100);
      }
    }


    // ---------- PCM lesen ----------
    size_t want = std::min(maxLen, (size_t)512);

    size_t n = 0;

    while (n == 0) {
      n = audioRingRead(buf, want);

      if (n == 0) {
        delay(1);
      }
    }


    // Nächsten Ausgabezeitpunkt festlegen.
    //
    // Bei 512 Byte sind das 16000 µs.
    //
    uint32_t durationUs =
      (uint32_t)(((uint64_t)n * 1000000ULL) / 32000ULL);

    _nextSendUs += durationUs;


    // ---------- Diagnose ----------
    static uint32_t dbgBytes = 0;
    static uint32_t dbgCalls = 0;
    static uint32_t dbgLast = 0;

    dbgBytes += n;
    dbgCalls++;

    uint32_t now = millis();

    if (now - dbgLast >= 1000) {
      uint32_t elapsed = now - dbgLast;

      Serial.printf(
        "[WAV] calls=%u bytes=%u rate=%u B/s ring=%u\n",
        (unsigned)dbgCalls,
        (unsigned)dbgBytes,
        (unsigned)((uint64_t)dbgBytes * 1000ULL / elapsed),
        (unsigned)gAudioUsed
      );

      dbgBytes = 0;
      dbgCalls = 0;
      dbgLast = now;
    }

    return n;
  }


private:
  uint8_t _header[44];
  size_t _headerPos;

  bool _started;
  uint32_t _nextSendUs;


  static void put16(uint8_t* p, uint16_t v) {
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
  }


  static void put32(uint8_t* p, uint32_t v) {
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
  }


  void buildHeader() {
    memset(_header, 0, sizeof(_header));

    memcpy(_header + 0, "RIFF", 4);

    // Praktisch endloser WAV-Stream
    put32(_header + 4, 0xFFFFFFFF);

    memcpy(_header + 8, "WAVE", 4);
    memcpy(_header + 12, "fmt ", 4);

    put32(_header + 16, 16);       // PCM fmt chunk
    put16(_header + 20, 1);        // PCM
    put16(_header + 22, 1);        // Mono
    put32(_header + 24, 16000);    // Sample Rate

    put32(_header + 28, 32000);    // Bytes pro Sekunde
    put16(_header + 32, 2);        // Block Align
    put16(_header + 34, 16);       // Bits/Sample

    memcpy(_header + 36, "data", 4);
    put32(_header + 40, 0xFFFFFFFF);
  }
};

// ================= RAW PCM AUDIO STREAM =================
class PCMResponse : public AsyncAbstractResponse {
public:
  PCMResponse()
    : _started(false),
      _nextSendUs(0) {

    _code = 200;
    _contentType = "application/octet-stream";
    _sendContentLength = false;
  }

  bool _sourceValid() const override {
    return true;
  }

  size_t _fillBuffer(uint8_t* buf, size_t maxLen) override {

    // Beim Start alten Audioinhalt wegwerfen und
    // ca. 200 ms frisches Audio sammeln.
    if (!_started) {

      audioRingFlush();

      while (true) {
        size_t used;

        taskENTER_CRITICAL(&audioMux);
        used = gAudioUsed;
        taskEXIT_CRITICAL(&audioMux);

        if (used >= 6400) {
          _started = true;
          _nextSendUs = micros();

          Serial.printf(
            "[PCM] start buffer=%u bytes\n",
            (unsigned)used
          );

          break;
        }

        delay(1);
      }
    }


    // 512 Byte = 16 ms Audio.
    // Ausgabe auf Echtzeitgeschwindigkeit begrenzen.
    uint32_t nowUs = micros();

    if ((int32_t)(nowUs - _nextSendUs) < 0) {
      uint32_t waitUs = _nextSendUs - nowUs;

      if (waitUs >= 1000) {
        delay(waitUs / 1000);
      }

      while ((int32_t)(micros() - _nextSendUs) < 0) {
        delayMicroseconds(100);
      }
    }


    size_t want = std::min(maxLen, (size_t)512);
    size_t n = 0;

    while (n == 0) {
      n = audioRingRead(buf, want);

      if (n == 0) {
        delay(1);
      }
    }


    // Zeitpunkt für nächsten PCM-Block:
    // 32000 Byte/s bei 16 kHz / 16 Bit / Mono.
    uint32_t durationUs =
      (uint32_t)(((uint64_t)n * 1000000ULL) / 32000ULL);

    _nextSendUs += durationUs;


    // Diagnose
    static uint32_t dbgBytes = 0;
    static uint32_t dbgCalls = 0;
    static uint32_t dbgLast = 0;

    dbgBytes += n;
    dbgCalls++;

    uint32_t now = millis();

    if (now - dbgLast >= 1000) {
      uint32_t elapsed = now - dbgLast;

      Serial.printf(
        "[PCM] calls=%u bytes=%u rate=%u B/s ring=%u\n",
        (unsigned)dbgCalls,
        (unsigned)dbgBytes,
        (unsigned)((uint64_t)dbgBytes * 1000ULL / elapsed),
        (unsigned)gAudioUsed
      );

      dbgBytes = 0;
      dbgCalls = 0;
      dbgLast = now;
    }

    return n;
  }

private:
  bool _started;
  uint32_t _nextSendUs;
};
// ================= UART TASK =================
void uartTask(void*) {
  uart_config_t cfg = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_APB
  };

  uart_param_config(UART_PORT, &cfg);
  uart_set_pin(UART_PORT, UART_PIN_NO_CHANGE, PIN_UART_RX,
               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(UART_PORT, 16384, 0, 0, NULL, 0);

  uint8_t rx[1024];

  for (;;) {
    int n = uart_read_bytes(UART_PORT, rx, sizeof(rx), 20 / portTICK_PERIOD_MS);
    if (n <= 0) continue;

    for (int i = 0; i < n; i++) {
      uint8_t b = rx[i];
      gBytes++;

      if (b == 0x00) {
        size_t dec = cobs_decode(encBuf, pktLen, decBuf);
        pktLen = 0;

        if (dec >= sizeof(pkt_hdr_t)) {
          pkt_hdr_t hdr;
          memcpy(&hdr, decBuf, sizeof(hdr));

          size_t need = sizeof(hdr) + hdr.payload_len;

          if (hdr.type == 0x01 && need == dec && hdr.payload_len > 0 && hdr.payload_len <= JPEG_MAX) {

            setFrame(decBuf + sizeof(hdr), hdr.payload_len);
            gFrames++;

            if ((gFrames % 30) == 0) {
              Serial.printf("[FRAME] frames=%u jpeg=%u\n",
                            (unsigned)gFrames, (unsigned)gLen);
            }

} else if (hdr.type == 0x02 &&
           need == dec &&
           hdr.payload_len > 0) {

  static uint32_t audioBytesPerSec = 0;
  static uint32_t lastAudioReport = 0;

  gAudioPackets++;
  gAudioBytes += hdr.payload_len;
  audioBytesPerSec += hdr.payload_len;
  audioRingWrite(decBuf + sizeof(hdr), hdr.payload_len);

  uint32_t now = millis();

  if (now - lastAudioReport >= 1000) {
    uint32_t elapsed = now - lastAudioReport;

    uint32_t bytesPerSec =
        (uint32_t)(((uint64_t)audioBytesPerSec * 1000ULL) / elapsed);

    uint32_t samplesPerSec =
        bytesPerSec / sizeof(int16_t);

    Serial.printf(
    "[AUDIO] packets=%u rate=%u B/s samples=%u/s ring=%u/%u\n",
    (unsigned)gAudioPackets,
    (unsigned)bytesPerSec,
    (unsigned)samplesPerSec,
    (unsigned)gAudioUsed,
    (unsigned)AUDIO_RING_SIZE);

    audioBytesPerSec = 0;
    lastAudioReport = now;
  }

} else {

            Serial.printf("[DROP] dec=%u type=%02X len=%u need=%u\n",
                          (unsigned)dec,
                          (unsigned)hdr.type,
                          (unsigned)hdr.payload_len,
                          (unsigned)need);
          }
        }
      } else {
        if (pktLen < JPEG_MAX + 256) {
          encBuf[pktLen++] = b;
        } else {
          pktLen = 0;
        }
      }
    }
  }
}

// ================= MJPEG =================
class MJPEGResponse : public AsyncAbstractResponse {
public:
  MJPEGResponse()
    : _state(0), _idx(0), _tmp(nullptr), _tmpLen(0) {
    _code = 200;
    _contentType = "multipart/x-mixed-replace; boundary=frame";
    _sendContentLength = false;
  }

  ~MJPEGResponse() {
    if (_tmp) free(_tmp);
  }

  bool _sourceValid() const override {
    return true;
  }

  size_t _fillBuffer(uint8_t* buf, size_t maxLen) override {
    switch (_state) {
      case 0:
        {
          static const char* s = "--frame\r\n";
          size_t L = strlen(s);
          size_t n = std::min(L - _idx, maxLen);
          memcpy(buf, s + _idx, n);
          _idx += n;
          if (_idx >= L) {
            _idx = 0;
            _state = 1;
          }
          return n;
        }

      case 1:
        {
          if (_tmp) {
            free(_tmp);
            _tmp = nullptr;
            _tmpLen = 0;
          }

          taskENTER_CRITICAL(&mux);
          size_t L = gLen;
          uint8_t* p = gJpeg;

          if (p && L) {
            _tmp = (uint8_t*)malloc(L);
            if (_tmp) {
              memcpy(_tmp, p, L);
              _tmpLen = L;
            }
          }
          taskEXIT_CRITICAL(&mux);

          _hdr = String("Content-Type: image/jpeg\r\nContent-Length: ") + String(_tmpLen) + "\r\n\r\n";

          size_t H = _hdr.length();
          size_t n = std::min(H - _idx, maxLen);
          memcpy(buf, _hdr.c_str() + _idx, n);
          _idx += n;
          if (_idx >= H) {
            _idx = 0;
            _state = 2;
          }
          return n;
        }

      case 2:
        {
          if (!_tmp || !_tmpLen) {
            _state = 0;
            delay(20);
            return 0;
          }

          size_t n = std::min(_tmpLen - _idx, maxLen);
          memcpy(buf, _tmp + _idx, n);
          _idx += n;

          if (_idx >= _tmpLen) {
            _idx = 0;
            _state = 3;
          }
          return n;
        }

      case 3:
        {
          static const char* s = "\r\n";
          size_t L = strlen(s);
          size_t n = std::min(L - _idx, maxLen);
          memcpy(buf, s + _idx, n);
          _idx += n;

          if (_idx >= L) {
            _idx = 0;
            _state = 0;
            delay(30);
          }
          return n;
        }
    }

    return 0;
  }

private:
  uint8_t _state;
  size_t _idx;
  String _hdr;
  uint8_t* _tmp;
  size_t _tmpLen;
};

// ================= BUTTON / STATE =================
void ringParty(int p) {
  if (p == 1) {
    stateP1 = CS_RING;
    ringSinceP1 = millis();
    Serial.println("[RING] P1");
  }

  if (p == 2) {
    stateP2 = CS_RING;
    ringSinceP2 = millis();
    Serial.println("[RING] P2");
  }

  if (p == 3) {
    stateP3 = CS_RING;
    ringSinceP3 = millis();
    Serial.println("[RING] P3");
  }
}

void handleButtons() {
  static bool lastP1 = HIGH;
  static bool lastP2 = HIGH;
  static bool lastP3 = HIGH;
  static uint32_t lastMs = 0;

  if (millis() - lastMs < BTN_DEBOUNCE_MS) return;
  lastMs = millis();

  bool nowP1 = digitalRead(PIN_BTN_P1);
  bool nowP2 = digitalRead(PIN_BTN_P2);
  bool nowP3 = digitalRead(PIN_BTN_P3);

  if (lastP1 == HIGH && nowP1 == LOW) ringParty(1);
  if (lastP2 == HIGH && nowP2 == LOW) ringParty(2);
  if (lastP3 == HIGH && nowP3 == LOW) ringParty(3);

  lastP1 = nowP1;
  lastP2 = nowP2;
  lastP3 = nowP3;
}

void handleTimeouts() {
  uint32_t now = millis();

  if (stateP1 == CS_RING && now - ringSinceP1 > RING_TIMEOUT_MS) stateP1 = CS_IDLE;
  if (stateP2 == CS_RING && now - ringSinceP2 > RING_TIMEOUT_MS) stateP2 = CS_IDLE;
  if (stateP3 == CS_RING && now - ringSinceP3 > RING_TIMEOUT_MS) stateP3 = CS_IDLE;
}

// ================= HELPERS =================
void listDir(File dir, String& out, const String& base = "/") {
  while (true) {
    File f = dir.openNextFile();
    if (!f) break;

    String path = String(base) + String(f.name());

    if (f.isDirectory()) {
      out += "DIR  " + path + "\n";
      File sub = LittleFS.open(path);
      listDir(sub, out, path + "/");
      sub.close();
    } else {
      out += "FILE " + path + " (" + String(f.size()) + " bytes)\n";
    }

    f.close();
  }
}

// ================= ROUTES =================
void setupRoutes() {
  server.on("/stream", HTTP_GET, [](AsyncWebServerRequest* r) {
    auto* res = new MJPEGResponse();
    res->addHeader("Cache-Control", "no-store");
    res->addHeader("Access-Control-Allow-Origin", "*");
    r->send(res);
  });

  server.on("/snapshot", HTTP_GET, [](AsyncWebServerRequest* r) {
    uint8_t* copy = nullptr;
    size_t L = 0;

    taskENTER_CRITICAL(&mux);
    if (gJpeg && gLen) {
      L = gLen;
      copy = (uint8_t*)malloc(L);
      if (copy) memcpy(copy, gJpeg, L);
    }
    taskEXIT_CRITICAL(&mux);

    if (!copy || !L) {
      if (copy) free(copy);
      r->send(503, "text/plain", "no frame");
      return;
    }

    AsyncWebServerResponse* res =
      r->beginResponse(200, "image/jpeg", copy, L);

    res->addHeader("Cache-Control", "no-store");
    r->send(res);

    free(copy);
  });


  server.on("/audio.wav", HTTP_GET, [](AsyncWebServerRequest* r) {
  auto* res = new WAVResponse();

  res->addHeader("Cache-Control", "no-store");
  res->addHeader("Access-Control-Allow-Origin", "*");

  r->send(res);
});

server.on("/audio.pcm", HTTP_GET, [](AsyncWebServerRequest* r) {
  auto* res = new PCMResponse();

  res->addHeader("Cache-Control", "no-store");
  res->addHeader("Access-Control-Allow-Origin", "*");

  r->send(res);
});

server.on("/audio.raw", HTTP_GET, [](AsyncWebServerRequest* r) {

  uint8_t* audio = (uint8_t*)malloc(65536);

  if (!audio) {
    r->send(500, "text/plain", "audio alloc failed");
    return;
  }

  size_t len = audioRingRead(audio, 65536);

  if (len == 0) {
    free(audio);
    r->send(503, "text/plain", "no audio");
    return;
  }

  AsyncWebServerResponse* res =
    r->beginResponse(200, "application/octet-stream", audio, len);

  res->addHeader("Cache-Control", "no-store");
  r->send(res);

  free(audio);
});
  server.on("/stats", HTTP_GET, [](AsyncWebServerRequest* r) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "heap=%u psram=%u jpeg=%u frames=%u bytes=%u p1=%s p2=%s p3=%s",
             ESP.getFreeHeap(),
             ESP.getFreePsram(),
             (unsigned)gLen,
             (unsigned)gFrames,
             (unsigned)gBytes,
             stateP1 == CS_RING ? "ring" : "idle",
             stateP2 == CS_RING ? "ring" : "idle",
             stateP3 == CS_RING ? "ring" : "idle");

    r->send(200, "text/plain", buf);
  });

  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* r) {
    String json = "{";
    json += "\"p1\":\"" + String(stateP1 == CS_RING ? "ring" : "idle") + "\",";
    json += "\"p2\":\"" + String(stateP2 == CS_RING ? "ring" : "idle") + "\",";
    json += "\"p3\":\"" + String(stateP3 == CS_RING ? "ring" : "idle") + "\"";
    json += "}";

    AsyncWebServerResponse* res =
      r->beginResponse(200, "application/json", json);

    res->addHeader("Cache-Control", "no-store");
    r->send(res);
  });

  server.on("/api/call/ack", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("party", true)) {
      r->send(400, "text/plain", "missing party");
      return;
    }

    int p = r->getParam("party", true)->value().toInt();

    if (p == 1) stateP1 = CS_IDLE;
    if (p == 2) stateP2 = CS_IDLE;
    if (p == 3) stateP3 = CS_IDLE;

    r->send(200, "text/plain", "OK");
  });

  server.on("/fs", HTTP_GET, [](AsyncWebServerRequest* r) {
    String out;
    File root = LittleFS.open("/");
    listDir(root, out, "/");
    root.close();
    r->send(200, "text/plain", out);
  });

  server.serveStatic("/", LittleFS, "/").setDefaultFile("idle.html");
  server.serveStatic("/p1/", LittleFS, "/p1/").setDefaultFile("index.html");
  server.serveStatic("/p2/", LittleFS, "/p2/").setDefaultFile("index.html");
  server.serveStatic("/p3/", LittleFS, "/p3/").setDefaultFile("index.html");
}

// ================= SETUP / LOOP =================
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("[BOOT] Doorstation Server clean no-audio");

  if (psramInit()) {
    Serial.printf("[PSRAM] OK: %u\n", ESP.getPsramSize());
  } else {
    Serial.println("[PSRAM] NOT FOUND");
  }

gAudioRing = (uint8_t*)ps_malloc(AUDIO_RING_SIZE);

if (!gAudioRing) {
  Serial.println("[AUDIO] ring buffer allocation FAILED");
  while (true) {
    delay(1000);
  }
}

Serial.printf("[AUDIO] ring buffer ready: %u bytes\n",
              (unsigned)AUDIO_RING_SIZE);


  pinMode(PIN_BTN_P1, INPUT_PULLUP);
  pinMode(PIN_BTN_P2, INPUT_PULLUP);
  pinMode(PIN_BTN_P3, INPUT_PULLUP);

  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed");
  } else {
    Serial.println("[FS] LittleFS mounted");
  }

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.printf("[WiFi] connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());

#if USE_MDNS
  if (MDNS.begin(MDNS_NAME)) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[mDNS] http://%s.local/\n", MDNS_NAME);
  }
#endif

  encBuf = (uint8_t*)ps_malloc(JPEG_MAX + 256);
  decBuf = (uint8_t*)ps_malloc(JPEG_MAX + 256);

  if (!encBuf || !decBuf) {
    Serial.println("[UART] buffer alloc failed");
    while (true) delay(1000);
  }

  xTaskCreatePinnedToCore(uartTask, "uart", 4096, NULL, 1, NULL, 1);

  setupRoutes();
  server.begin();

  Serial.println("[HTTP] ready");
}

void loop() {
  handleButtons();
  handleTimeouts();
  delay(10);
}