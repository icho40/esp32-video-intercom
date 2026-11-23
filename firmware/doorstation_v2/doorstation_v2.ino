#include <Arduino.h>
#include "Config.h"
#include <atomic>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "driver/uart.h"
#include "driver/i2s_std.h"
#include <math.h>
#include "freertos/queue.h"
static void printOtaAccess();
static bool setOtaPassword(const String& password);
#include "WifiSetup.h"
#include "BootWifiSelection.h"
#include "WifiProfileRecovery.h"
#include "WifiRoaming.h"

#define UART_PORT   UART_NUM_1
#define PIN_UART_RX 18
#define UART_BAUD   2000000  // muss zu VIDEO_BAUD der XIAO passen

#define PIN_BUTTON_P1 4
#define PIN_BUTTON_P2 5
#define PIN_BUTTON_P3 6

enum class IntercomState : uint8_t {
  IDLE,
  RINGING_P1,
  RINGING_P2,
  RINGING_P3,
  TALKING
};

static IntercomState intercomState = IntercomState::IDLE;
static uint32_t stateGeneration = 0;
static bool wifiSetupRequested = false;
static uint8_t activeButton = 0;
static uint32_t ringingStartedAt = 0;
static std::atomic<uint32_t> ringWaitMs{30000};  // Wartezeit bis Ansage/Ende, siehe Announcement.h
// Ansage bei unbeantwortetem Ruf: loop() fordert sie fuer eine Ruf-Generation an,
// der Lautsprecher-Task spielt sie ab und meldet die Generation als erledigt.
static std::atomic<uint32_t> annRequestGen{0}, annDoneGen{0};
static std::atomic<uint8_t> annRequestId{0};  // Satz aus der Sammlung
static std::atomic<uint32_t> annVersion{1};   // Sammlung/Auswahl geaendert -> an Tablets senden
static String annControlLines();
static bool annSetFloor(uint8_t button, uint8_t sel, bool instant, uint32_t until);
static void annSetClockFromClient(uint32_t epoch);
// Verpasste Rufe: Zaehler + Klingel, controlTask sendet "MISSED Pn" an alle Tablets.
static std::atomic<uint32_t> missedCount{0};
static std::atomic<uint8_t> missedButton{0};
static std::atomic<uint32_t> missedCall{0};  // Ruf-Kennung (Generation) fuer /api/missed.jpg
static portMUX_TYPE stateMux = portMUX_INITIALIZER_UNLOCKED;

static IntercomState getIntercomState() {
  portENTER_CRITICAL(&stateMux);
  IntercomState state = intercomState;
  portEXIT_CRITICAL(&stateMux);

  return state;
}

struct StateSnapshot {
  IntercomState state;
  uint32_t generation;
  uint8_t button;
};

static StateSnapshot getStateSnapshot();
static void sendControlState(WiFiClient& client, const StateSnapshot& state);

static StateSnapshot getStateSnapshot() {
  StateSnapshot snapshot;

  portENTER_CRITICAL(&stateMux);
  snapshot.state = intercomState;
  snapshot.generation = stateGeneration;
  snapshot.button = activeButton;
  portEXIT_CRITICAL(&stateMux);

  return snapshot;
}

static const char* intercomStateName(IntercomState state) {
  switch (state) {
    case IntercomState::IDLE:
      return "IDLE";

    case IntercomState::RINGING_P1:
      return "RINGING_P1";

    case IntercomState::RINGING_P2:
      return "RINGING_P2";

    case IntercomState::RINGING_P3:
      return "RINGING_P3";

    case IntercomState::TALKING:
      return "TALKING";

    default:
      return "UNKNOWN";
  }
}

static bool changeIntercomState(
    IntercomState expected,
    IntercomState next
) {
  bool changed = false;

  portENTER_CRITICAL(&stateMux);

  if (intercomState == expected) {
    intercomState = next;
    activeButton = next == IntercomState::RINGING_P1 ? 1 :
        next == IntercomState::RINGING_P2 ? 2 :
        next == IntercomState::RINGING_P3 ? 3 : 0;
    ringingStartedAt = millis();
    stateGeneration++;
    changed = true;
  }

  portEXIT_CRITICAL(&stateMux);

  return changed;
}

static bool answerIntercom(uint8_t button) {
  bool changed = false;

  portENTER_CRITICAL(&stateMux);

  if (button && activeButton == button && (intercomState == IntercomState::RINGING_P1 ||
      intercomState == IntercomState::RINGING_P2 ||
      intercomState == IntercomState::RINGING_P3)) {
    intercomState = IntercomState::TALKING;
    stateGeneration++;
    changed = true;
  }

  portEXIT_CRITICAL(&stateMux);

  return changed;
}

static bool hangupIntercom(uint8_t button) {
  bool changed = false;

  portENTER_CRITICAL(&stateMux);

  if (button && activeButton == button && (intercomState == IntercomState::RINGING_P1 ||
      intercomState == IntercomState::RINGING_P2 ||
      intercomState == IntercomState::RINGING_P3 ||
      intercomState == IntercomState::TALKING)) {
    intercomState = IntercomState::IDLE;
    activeButton = 0;
    stateGeneration++;
    changed = true;
  }

  portEXIT_CRITICAL(&stateMux);

  return changed;
}

#define JPEG_MAX (120 * 1024)

struct __attribute__((packed)) PacketHeader {
  uint8_t  type;
  uint16_t seq;
  uint32_t timestamp_ms;
  uint32_t payload_len;
};

static_assert(sizeof(PacketHeader) == 11,
              "PacketHeader must be exactly 11 bytes");

static uint8_t* encBuf = nullptr;
static uint8_t* decBuf = nullptr;
static size_t pktLen = 0;

static std::atomic<uint32_t> jpegPackets{0};
static std::atomic<uint32_t> jpegBytes{0};
static std::atomic<uint32_t> audioPackets{0};
static std::atomic<uint32_t> audioBytes{0};
static std::atomic<uint32_t> droppedPackets{0};
static std::atomic<uint32_t> droppedAudioQueue{0};  // Teil von droppedPackets
static std::atomic<uint32_t> audioUnderruns{0};
static std::atomic<uint32_t> uartOverflows{0}, uartFrameErrors{0};
static QueueHandle_t uartEvents = nullptr;

static uint32_t lastReport = 0;
static uint32_t lastWiFiCheck = 0;
static uint32_t wifiRoamCount = 0;

#define TCP_VIDEO_PORT   5000
#define TCP_CONTROL_PORT 5001

static uint8_t* jpegNetBuf = nullptr;
static size_t jpegNetLen = 0;
static uint32_t jpegNetGeneration = 0;
static uint32_t lastJpegAt = 0;
static std::atomic<uint32_t> uartBytes{0};

static portMUX_TYPE jpegMux = portMUX_INITIALIZER_UNLOCKED;  // nur kurze Felder
// Die grossen Bildkopien laufen unter einem Mutex statt im kritischen Abschnitt:
// Ein 12-kB-memcpy mit gesperrten Interrupts liess bei 2 Mbaud die UART-FIFO ueberlaufen.
static SemaphoreHandle_t jpegLock = nullptr;

// Bild je Klingel ca. 2 s nach dem Klingeln, fuer "verpasster Besuch"
// (abrufbar ueber /api/missed.jpg?p=N&call=ID, nur im lokalen Netz).
static uint8_t* ringSnap[4] = {};
static size_t ringSnapLen[4] = {};
static uint32_t ringSnapCall[4] = {};
static constexpr uint32_t RING_SNAPSHOT_DELAY_MS = 2000;

// Nur aus loop() (wie die HTTP-Handler), daher ohne weitere Sperre.
static void captureRingSnapshot(uint8_t button, uint32_t call) {
  if (button < 1 || button > 3 || !ringSnap[button] || ringSnapCall[button] == call) return;
  if (xSemaphoreTake(jpegLock, pdMS_TO_TICKS(20)) != pdTRUE) return;
  bool fresh = jpegNetLen > 0 && (uint32_t)(millis() - lastJpegAt) < 1500;
  if (fresh) {
    memcpy(ringSnap[button], jpegNetBuf, jpegNetLen);
    ringSnapLen[button] = jpegNetLen;
    ringSnapCall[button] = call;
  }
  xSemaphoreGive(jpegLock);
}

static WiFiServer videoServer(TCP_VIDEO_PORT);
static WiFiServer controlServer(TCP_CONTROL_PORT);

#define UDP_AUDIO_PORT 5002
static WiFiUDP audioUdp;

static constexpr int MAX_TABLETS = 8;
struct TabletRoute { IPAddress ip; uint8_t button; bool connected; };
static TabletRoute tabletRoutes[MAX_TABLETS];
static portMUX_TYPE routeMux = portMUX_INITIALIZER_UNLOCKED;

static uint8_t calledButton() {
  portENTER_CRITICAL(&stateMux);
  uint8_t button = activeButton;
  portEXIT_CRITICAL(&stateMux);
  return button;
}

static bool isCalledTablet(IPAddress ip) {
  uint8_t button = calledButton();
  bool allowed = false;
  portENTER_CRITICAL(&routeMux);
  for (int i = 0; i < MAX_TABLETS; ++i) {
    if (button && tabletRoutes[i].connected && tabletRoutes[i].button == button &&
        tabletRoutes[i].ip == ip) allowed = true;
  }
  portEXIT_CRITICAL(&routeMux);
  return allowed;
}

// XIAO: 256 PCM-Samples = 512 Bytes pro Audiopaket
#define AUDIO_PACKET_MAX 512
#define AUDIO_QUEUE_LENGTH 32  // ca. 0,5 s; nimmt die Ton-Schwaelle nach jedem Bild auf

struct AudioPacket {
  uint16_t length;
  uint8_t data[AUDIO_PACKET_MAX];
};

static QueueHandle_t audioQueue = nullptr;
static bool wifiSleepDisableAccepted = false;
#include "OtaUpdate.h"
#include "AudioGain.h"
#include "Diagnostics.h"


// ------------------------------------------------------------
// WLAN
// ------------------------------------------------------------

static bool bootButtonsArmed = false;
static void startWiFi() {
  loadWifiSettings();
  Serial.println("[WIFI SELECT] 20s: P1=" TEST_WIFI_SSID ", P2=" HOME_WIFI_SSID "; no button=last successful WLAN");
  BootWifiSelection selection(millis());
  uint8_t choice = 0;
  while (!(choice = selection.update(millis(), digitalRead(PIN_BUTTON_P1) == LOW,
                                     digitalRead(PIN_BUTTON_P2) == LOW))) delay(5);
  String selected = choice == 1 ? String(TEST_WIFI_SSID) : choice == 2 ? String(HOME_WIFI_SSID) : lastSuccessfulWifi;
  if (!selected.length() || (selected != configuredSsid && selected != additionalSsid)) {
    if (choice != 3) Serial.println("[WIFI SELECT] Selected profile not stored; using saved default");
    selected = configuredSsid.length() ? configuredSsid : additionalSsid;
  }
  usingAdditionalWifi = selected.length() && selected == additionalSsid;
  Serial.printf("[WIFI SELECT] selected=%s reason=%s\n", selected.c_str(),
                choice == 3 ? "timeout" : "button");
  Serial.printf("[WIFI] Connecting to %s", selected.c_str());

  WiFi.mode(WIFI_STA);
  // No modem sleep: otherwise incoming packets are missed between beacons
  // (ping loss, HTTP timeouts). The setting is kept across later mode changes.
  wifiSleepDisableAccepted = WiFi.setSleep(false);
  Serial.printf("[WIFI] disable sleep accepted=%s\n", wifiSleepDisableAccepted ? "true" : "false");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  // Alle Kanaele absuchen und den staerksten Zugangspunkt nehmen. Standard ist
  // WIFI_FAST_SCAN: der erste gefundene Repeater gewinnt, egal wie schwach.
  // Gilt fuer alle spaeteren WiFi.begin()-Aufrufe und das automatische Wiederverbinden.
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  if (selected.length()) WiFi.begin(selected.c_str(),
      usingAdditionalWifi ? additionalPassword.c_str() : configuredPassword.c_str());

  uint32_t start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 15000) {
    Serial.print(".");
    delay(250);
  }

  Serial.println();

  const String fallback = usingAdditionalWifi ? configuredSsid : additionalSsid;
  if (WiFi.status() != WL_CONNECTED && fallback.length() && fallback != selected) {
    Serial.printf("[WIFI] Trying other saved profile %s\n", fallback.c_str());
    usingAdditionalWifi = !usingAdditionalWifi;
    WiFi.begin(fallback.c_str(), usingAdditionalWifi ? additionalPassword.c_str() : configuredPassword.c_str());
    uint32_t additionalStart = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - additionalStart < 15000) delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    rememberSuccessfulWifi();
    Serial.printf(
        "[WIFI] Connected: IP=%s  RSSI=%d dBm\n",
        WiFi.localIP().toString().c_str(),
        WiFi.RSSI()
    );
  } else {
    Serial.println(
        "[WIFI] Initial connection failed - retrying in background"
    );
    startWifiSetup();
  }
}


static void checkWiFi() {
  static WifiProfileRecovery recovery;
  bool connected = WiFi.status() == WL_CONNECTED;
  WifiRecoveryAction action = recovery.update(millis(), connected, candidateActive);
  if (candidateActive) return;
  if (connected) {
    rememberSuccessfulWifi();
    usingAdditionalWifi = additionalSsid.length() && WiFi.SSID() == additionalSsid;
    return;
  }
  if (action.openSetup && !wifiSetupActive) startWifiSetup();
  if (action.tryOther) {
    usingAdditionalWifi = !usingAdditionalWifi && additionalSsid.length() && additionalSsid != configuredSsid;
    const String& ssid = usingAdditionalWifi ? additionalSsid : configuredSsid;
    const String& pass = usingAdditionalWifi ? additionalPassword : configuredPassword;
    Serial.printf("[WIFI] Trying saved profile %s\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), pass.c_str());
  }
}

// Wechselt im Ruhezustand zu einem deutlich staerkeren Repeater desselben WLANs.
// Der ESP32 wechselt von sich aus nie, solange die schwache Verbindung haelt.
static void handleWifiRoaming() {
  static WifiRoamingPolicy policy;
  static bool scanning = false;
  static bool roamPending = false;
  static bool bssidPinned = false;
  static uint32_t roamStarted = 0;
  static uint32_t lastPoll = 0;
  static String roamSsid, roamPassword;

  uint32_t now = millis();
  if ((uint32_t)(now - lastPoll) < 1000) return;
  lastPoll = now;

  bool connected = WiFi.status() == WL_CONNECTED;
  bool idle = getIntercomState() == IntercomState::IDLE &&
              !wifiSetupActive && !candidateActive;

  if (roamPending) {
    if (connected) {
      roamPending = false;
      wifiRoamCount++;
      Serial.printf("[WIFI ROAM] Verbunden mit %s, RSSI=%d dBm\n",
                    WiFi.BSSIDstr().c_str(), WiFi.RSSI());
    } else if ((uint32_t)(now - roamStarted) >= 15000) {
      roamPending = false;
      bssidPinned = false;
      Serial.println("[WIFI ROAM] Wechsel fehlgeschlagen, verbinde ohne feste BSSID");
      WiFi.begin(roamSsid.c_str(), roamPassword.c_str());
    }
    return;
  }

  // Nach einem Wechsel ist die BSSID fest eingetragen. Faellt dieser Repeater
  // spaeter aus, wieder frei mit dem staerksten verbinden.
  if (bssidPinned && !connected && !candidateActive) {
    bssidPinned = false;
    Serial.println("[WIFI ROAM] Verbindung verloren, BSSID-Bindung aufgehoben");
    WiFi.begin(roamSsid.c_str(), roamPassword.c_str());
    return;
  }

  if (scanning) {
    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    scanning = false;
    if (n <= 0 || !connected || !idle) { WiFi.scanDelete(); return; }

    String ssid = WiFi.SSID();
    int32_t currentRssi = WiFi.RSSI();
    uint8_t currentBssid[6] = {0};
    WiFi.BSSID(currentBssid);

    WifiRoamingCandidate list[20];
    int count = n < 20 ? n : 20;
    for (int i = 0; i < count; i++) {
      memcpy(list[i].bssid, WiFi.BSSID(i), 6);
      list[i].rssi = WiFi.RSSI(i);
      list[i].channel = WiFi.channel(i);
      list[i].sameSsid = WiFi.SSID(i) == ssid;
    }
    WiFi.scanDelete();

    int best = WifiRoamingPolicy::pickCandidate(currentBssid, currentRssi, list, count);
    if (best < 0) {
      Serial.printf("[WIFI ROAM] Kein deutlich besserer Repeater (aktuell %d dBm)\n", currentRssi);
      return;
    }
    roamSsid = ssid;
    roamPassword = (additionalSsid.length() && ssid == additionalSsid) ? additionalPassword
                                                                        : configuredPassword;
    Serial.printf("[WIFI ROAM] Wechsel %d dBm -> %d dBm (Kanal %ld)\n",
                  currentRssi, (int)list[best].rssi, (long)list[best].channel);
    roamPending = true;
    bssidPinned = true;
    roamStarted = now;
    WiFi.begin(roamSsid.c_str(), roamPassword.c_str(), list[best].channel, list[best].bssid);
    return;
  }

  if (policy.shouldScan(now, connected, idle, connected ? WiFi.RSSI() : -127)) {
    Serial.printf("[WIFI ROAM] Signal schwach (%d dBm), suche staerkeren Repeater\n", WiFi.RSSI());
    String ssid = WiFi.SSID();
    if (WiFi.scanNetworks(true, false, false, 120, 0, ssid.c_str()) != WIFI_SCAN_FAILED)
      scanning = true;
  }
}


// ------------------------------------------------------------
// COBS
// ------------------------------------------------------------

static size_t cobsDecode(const uint8_t* in,
                         size_t len,
                         uint8_t* out) {
  const uint8_t* end = in + len;
  size_t o = 0;

  while (in < end) {
    uint8_t code = *in++;

    if (code == 0) {
      return 0;
    }

    for (uint8_t i = 1; i < code; i++) {
      if (in >= end) {
        return 0;
      }

      out[o++] = *in++;
    }

    if (code < 0xFF && in < end) {
      out[o++] = 0x00;
    }
  }

  return o;
}


// ------------------------------------------------------------
// XIAO packet processing
// ------------------------------------------------------------

static void processPacket(size_t decodedLen) {
  if (decodedLen < sizeof(PacketHeader)) {
    droppedPackets++;
    return;
  }

  PacketHeader hdr;
  memcpy(&hdr, decBuf, sizeof(hdr));

  size_t expected =
      sizeof(PacketHeader) + (size_t)hdr.payload_len;

  if (expected != decodedLen) {
    droppedPackets++;
    return;
  }

  switch (hdr.type) {

    case 0x01:
      if (hdr.payload_len == 0 ||
          hdr.payload_len > JPEG_MAX) {
        droppedPackets++;
        return;
      }

      jpegPackets++;
      jpegBytes += hdr.payload_len;

      xSemaphoreTake(jpegLock, portMAX_DELAY);
      memcpy(
          jpegNetBuf,
          decBuf + sizeof(PacketHeader),
          hdr.payload_len
      );
      portENTER_CRITICAL(&jpegMux);
      jpegNetLen = hdr.payload_len;
      jpegNetGeneration++;
      lastJpegAt = millis();
      portEXIT_CRITICAL(&jpegMux);
      xSemaphoreGive(jpegLock);

      break;

    case 0x02:
      if (hdr.payload_len == 0 ||
          hdr.payload_len > AUDIO_PACKET_MAX ||
          (hdr.payload_len % sizeof(int16_t)) != 0) {
        droppedPackets++;
        return;
      }

      audioPackets++;
      audioBytes += hdr.payload_len;

      if (audioQueue != nullptr) {
        AudioPacket packet;
        packet.length = (uint16_t)hdr.payload_len;

        memcpy(
            packet.data,
            decBuf + sizeof(PacketHeader),
            packet.length
        );

        if (xQueueSend(audioQueue, &packet, 0) != pdTRUE) {
          // Warteschlange voll: UART nicht blockieren
          droppedPackets++;
          droppedAudioQueue++;
        }
      }

      break;

    default:
      droppedPackets++;
      break;
  }
}


// ------------------------------------------------------------
// UART receiver
// ------------------------------------------------------------

static void uartTask(void*) {
  uart_config_t cfg = {
    .baud_rate = UART_BAUD,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
    .source_clk = UART_SCLK_APB
  };

  ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));

  ESP_ERROR_CHECK(
      uart_set_pin(
          UART_PORT,
          UART_PIN_NO_CHANGE,
          PIN_UART_RX,
          UART_PIN_NO_CHANGE,
          UART_PIN_NO_CHANGE
      )
  );

  ESP_ERROR_CHECK(
      uart_driver_install(
          UART_PORT,
          32768,  // 2 Mbaud: ca. 160 ms Puffer
          0,
          32,
          &uartEvents,
          0
      )
  );
  // Bei 2 Mbaud ist die 128-Byte-FIFO in 0,64 ms voll. Frueher abholen als die
  // Vorgabe (120 Bytes = nur 40 us Reserve), sonst gehen bei kurzen Sperren Bytes verloren.
  uart_set_rx_full_threshold(UART_PORT, 64);
  uart_set_rx_timeout(UART_PORT, 4);

  uint8_t rx[1024];

  Serial.printf(
      "[UART] RX GPIO%d @ %u Baud\n",
      PIN_UART_RX,
      (unsigned)UART_BAUD
  );

  for (;;) {
    int n = uart_read_bytes(
        UART_PORT,
        rx,
        sizeof(rx),
        pdMS_TO_TICKS(20)
    );

    uart_event_t event;
    while (uartEvents && xQueueReceive(uartEvents, &event, 0) == pdTRUE) {
      if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
        uartOverflows++;  // nur zaehlen; das kaputte Paket verwirft der COBS-Pruefer
      } else if (event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR) {
        uartFrameErrors++;
      }
    }

    if (n <= 0) {
      continue;
    }

    static uint32_t lastUartReport = 0;

    uartBytes += n;

    uint32_t now = millis();
    if (now - lastUartReport >= 5000) {
      Serial.printf("[UART] RX bytes=%lu\n",
                    (unsigned long)uartBytes);
      lastUartReport = now;
    }

    for (int i = 0; i < n; i++) {
      uint8_t b = rx[i];

      if (b == 0x00) {
        if (pktLen == 0) {
          continue;
        }

        size_t decodedLen =
            cobsDecode(encBuf, pktLen, decBuf);

        pktLen = 0;

        if (decodedLen == 0) {
          droppedPackets++;
          continue;
        }

        processPacket(decodedLen);

      } else {
        if (pktLen < JPEG_MAX + 256) {
          encBuf[pktLen++] = b;
        } else {
          pktLen = 0;
          droppedPackets++;
        }
      }
    }
  }
}


// ------------------------------------------------------------
// TCP video server
// ------------------------------------------------------------

// Mehrere Geraete gleichzeitig (z. B. EG-Tablet und Handy an P1): jedes neue
// Bild geht an alle verbundenen Clients. Frueher bediente der Server nur einen
// Client; weitere warteten, bis der erste trennte.
static constexpr int MAX_VIDEO_CLIENTS = 4;

static void videoTask(void*) {
  videoServer.begin();
  videoServer.setNoDelay(true);

  Serial.printf(
      "[VIDEO] TCP server listening on port %u (max %d clients)\n",
      (unsigned)TCP_VIDEO_PORT, MAX_VIDEO_CLIENTS
  );

  uint8_t* sendBuf = (uint8_t*)ps_malloc(JPEG_MAX);
  while (!sendBuf) {
    Serial.println("[VIDEO] Cannot allocate send buffer");
    delay(1000);
    sendBuf = (uint8_t*)ps_malloc(JPEG_MAX);
  }

  WiFiClient clients[MAX_VIDEO_CLIENTS];
  uint32_t sentGeneration[MAX_VIDEO_CLIENTS] = {};
  bool active[MAX_VIDEO_CLIENTS] = {};
  size_t bufLen = 0;
  uint32_t bufGeneration = 0;

  for (;;) {
    WiFiClient incoming = videoServer.available();
    if (incoming) {
      int slot = -1;
      for (int i = 0; i < MAX_VIDEO_CLIENTS; ++i) {
        if (!active[i]) { slot = i; break; }
      }
      if (slot < 0) {
        Serial.printf("[VIDEO] Rejected %s: all %d slots busy\n",
                      incoming.remoteIP().toString().c_str(), MAX_VIDEO_CLIENTS);
        incoming.stop();
      } else {
        clients[slot].stop();
        clients[slot] = incoming;
        clients[slot].setNoDelay(true);
        sentGeneration[slot] = 0;
        active[slot] = true;
        Serial.printf("[VIDEO] Client connected: %s (slot %d)\n",
                      clients[slot].remoteIP().toString().c_str(), slot);
      }
    }

    // Neuestes Bild einmal kopieren, dann an alle verteilen.
    xSemaphoreTake(jpegLock, portMAX_DELAY);
    if (jpegNetGeneration != bufGeneration && jpegNetLen > 0) {
      bufLen = jpegNetLen;
      memcpy(sendBuf, jpegNetBuf, bufLen);
      bufGeneration = jpegNetGeneration;
    }
    xSemaphoreGive(jpegLock);

    bool sent = false;
    for (int i = 0; i < MAX_VIDEO_CLIENTS; ++i) {
      if (!active[i]) continue;
      if (!clients[i].connected()) {
        clients[i].stop();
        active[i] = false;
        Serial.printf("[VIDEO] Client disconnected (slot %d)\n", i);
        continue;
      }
      if (bufLen == 0 || sentGeneration[i] == bufGeneration) continue;

      uint32_t len = (uint32_t)bufLen;
      if (clients[i].write((const uint8_t*)&len, sizeof(len)) != sizeof(len) ||
          clients[i].write(sendBuf, bufLen) != bufLen) {
        clients[i].stop();
        active[i] = false;
        Serial.printf("[VIDEO] Write failed, client dropped (slot %d)\n", i);
        continue;
      }
      sentGeneration[i] = bufGeneration;
      sent = true;
    }

    if (!sent) delay(2);
  }
}


// ------------------------------------------------------------
// TCP control server
// ------------------------------------------------------------

static void sendControlState(WiFiClient& client, const StateSnapshot& state) {
  if (state.state == IntercomState::TALKING) {
    client.printf("STATE TALKING_P%u\n", state.button);
  } else {
    client.printf("STATE %s\n", intercomStateName(state.state));
  }
}

static void controlTask(void*) {
  WiFiClient clients[MAX_TABLETS];
  String lines[MAX_TABLETS];
  bool overflow[MAX_TABLETS] = {};
  uint32_t generations[MAX_TABLETS] = {};
  uint32_t lastRx[MAX_TABLETS] = {};
  uint8_t buttons[MAX_TABLETS] = {};
  uint32_t missedSeen[MAX_TABLETS] = {};
  uint32_t annSeen[MAX_TABLETS] = {};
  controlServer.begin();
  controlServer.setNoDelay(true);
  Serial.println("[CONTROL] Multi-tablet server on port 5001");
  for (;;) {
    WiFiClient incoming = controlServer.available();
    if (incoming) {
      int slot = -1;
      for (int i = 0; i < MAX_TABLETS; ++i) if (!clients[i].connected()) { slot = i; break; }
      if (slot < 0) incoming.stop();
      else {
        clients[slot].stop();
        clients[slot] = incoming;
        clients[slot].setNoDelay(true);
        clients[slot].setTimeout(100);
        lines[slot] = ""; overflow[slot] = false; buttons[slot] = 0;
        lastRx[slot] = millis();
        IPAddress remote = incoming.remoteIP();
        portENTER_CRITICAL(&routeMux);
        tabletRoutes[slot] = {remote, 0, true};
        portEXIT_CRITICAL(&routeMux);
        clients[slot].println("HELLO INTERCOM_V2");
        StateSnapshot state = getStateSnapshot();
        sendControlState(clients[slot], state);
        generations[slot] = state.generation;
        missedSeen[slot] = missedCount;
        annSeen[slot] = 0;  // Satzsammlung und Auswahl gleich senden
        Serial.printf("[CONTROL] Connected slot=%d ip=%s\n", slot, incoming.remoteIP().toString().c_str());
      }
    }
    for (int i = 0; i < MAX_TABLETS; ++i) {
      if (!clients[i].connected() || millis() - lastRx[i] > 45000) {
        clients[i].stop();
        portENTER_CRITICAL(&routeMux);
        tabletRoutes[i].connected = false;
        portEXIT_CRITICAL(&routeMux);
        continue;
      }
      // Begrenzte, nicht blockierende Zeilenverarbeitung pro Tablet.
      int budget = 128;
      while (budget-- && clients[i].available()) {
        char c = (char)clients[i].read();
        lastRx[i] = millis();
        if (c == '\n') {
          String line = lines[i]; lines[i] = "";
          if (overflow[i]) { overflow[i] = false; continue; }
          line.trim();
          if (line.length() == 11 && line.startsWith("REGISTER P") && line[10] >= '1' && line[10] <= '3') {
            buttons[i] = line[10] - '0';
            portENTER_CRITICAL(&routeMux);
            tabletRoutes[i].button = buttons[i];
            portEXIT_CRITICAL(&routeMux);
            clients[i].printf("REGISTERED P%u\n", buttons[i]);
            Serial.printf("[CONTROL] slot=%d registered P%u\n", i, buttons[i]);
          } else if (line == "WIFI_SETUP") {
            bool allowed = false;
            portENTER_CRITICAL(&stateMux);
            if (buttons[i] && intercomState == IntercomState::IDLE) {
              wifiSetupRequested = true;
              allowed = true;
            }
            portEXIT_CRITICAL(&stateMux);
            clients[i].println(allowed ? "SETUP STARTING" : "ERROR SETUP_BUSY");
          } else if (line == "WIFI_RSSI") {
            if (WiFi.status() == WL_CONNECTED) clients[i].printf("WIFI_RSSI %d\n", WiFi.RSSI());
            else clients[i].println("WIFI_RSSI DISCONNECTED");
          } else if (line.startsWith("ANNSET ")) {
            // "ANNSET <Satz> <sofort 0|1> <Ende Unix-Zeit>" fuer die eigene Klingel
            unsigned sel = 0, instant = 0; unsigned long until = 0;
            if (buttons[i] && sscanf(line.c_str() + 7, "%u %u %lu", &sel, &instant, &until) == 3 &&
                annSetFloor(buttons[i], (uint8_t)sel, instant != 0, (uint32_t)until)) {
              clients[i].println("ANNSET OK");
            } else clients[i].println("ERROR ANNSET");
          } else if (line.startsWith("TIME ")) {
            annSetClockFromClient((uint32_t)strtoul(line.c_str() + 5, nullptr, 10));
          } else if (line == "PING") {
            clients[i].println("PONG");
          } else if (line == "HANGUP" || line == "ANSWER") {
            if (buttons[i] && buttons[i] == calledButton()) {
              bool changed = line == "HANGUP" ? hangupIntercom(buttons[i]) : answerIntercom(buttons[i]);
              if (!changed) clients[i].println("STATE UNCHANGED");
            } else clients[i].println("ERROR NOT_YOUR_CALL");
          }
        } else if (c != '\r') {
          if (lines[i].length() < 64 && !overflow[i]) lines[i] += c;
          else overflow[i] = true;
        }
      }
      StateSnapshot state = getStateSnapshot();
      if (generations[i] != state.generation) {
        sendControlState(clients[i], state);
        generations[i] = state.generation;
      }
      uint32_t missed = missedCount;
      if (missedSeen[i] != missed) {
        missedSeen[i] = missed;
        clients[i].printf("MISSED P%u %lu\n", missedButton.load(), (unsigned long)missedCall.load());
      }
      uint32_t annNow = annVersion;
      if (annSeen[i] != annNow) {
        annSeen[i] = annNow;
        clients[i].print(annControlLines());
      }
    }
    // Nur das Fehlen aller passenden Tablets beendet ein Gespräch.
    // Fremde Tablets und der normale 30-s-Klingelzustand bleiben unberührt.
    StateSnapshot conversation = getStateSnapshot();
    if (conversation.state == IntercomState::TALKING) {
      bool reachable = false;
      portENTER_CRITICAL(&routeMux);
      for (int i = 0; i < MAX_TABLETS; ++i) {
        if (tabletRoutes[i].connected &&
            tabletRoutes[i].button == conversation.button) {
          reachable = true;
          break;
        }
      }
      portEXIT_CRITICAL(&routeMux);
      bool released = false;
      if (!reachable) {
        portENTER_CRITICAL(&stateMux);
        // Der vorher geprüfte Ruf muss noch derselbe sein.
        if (intercomState == IntercomState::TALKING &&
            activeButton == conversation.button &&
            stateGeneration == conversation.generation) {
          intercomState = IntercomState::IDLE;
          activeButton = 0;
          stateGeneration++;
          released = true;
        }
        portEXIT_CRITICAL(&stateMux);
      }
      if (released) Serial.printf(
          "[STATE] TALKING_P%u -> IDLE: no matching tablet connected\n",
          conversation.button);
    }
    delay(2);
  }
}


// ------------------------------------------------------------
// UDP audio sender
// ------------------------------------------------------------

static void audioUdpTask(void* parameter) {
  AudioPacket packet;

  audioUdp.begin(UDP_AUDIO_PORT);
  Serial.println("[AUDIO] UDP sender started");

  uint32_t udpSent = 0;
  uint32_t udpErrors = 0;
  uint32_t udpSkipped = 0;
  uint32_t lastUdpReport = millis();

  // Die XIAO schickt jedes Bild am Stueck (bei 640x480 ca. 60 ms); der Ton kommt
  // danach schubweise. Die Apps spielen ohne eigenen Vorrat ab, darum hier ca. 96 ms
  // vorpuffern und gleichmaessig im Takt der Pakete (256 Samples = 16 ms) senden.
  static constexpr UBaseType_t AUDIO_PREBUFFER = 6;
  static constexpr UBaseType_t AUDIO_MAX_BACKLOG = 16;  // darueber Verzoegerung abbauen
  bool buffering = true;
  TickType_t nextSend = xTaskGetTickCount();

  while (true) {
    if (buffering) {
      if (uxQueueMessagesWaiting(audioQueue) < AUDIO_PREBUFFER) {
        vTaskDelay(pdMS_TO_TICKS(2));
        continue;
      }
      buffering = false;
      nextSend = xTaskGetTickCount();
    }
    if (xQueueReceive(audioQueue, &packet, 0) != pdTRUE) {
      buffering = true;
      audioUnderruns++;
      continue;
    }

    IPAddress targets[MAX_TABLETS];
    int count = 0;
    uint8_t button = calledButton();
    portENTER_CRITICAL(&routeMux);
    for (int i = 0; i < MAX_TABLETS; ++i) {
      if (button && tabletRoutes[i].connected && tabletRoutes[i].button == button) {
        targets[count++] = tabletRoutes[i].ip;
      }
    }
    portEXIT_CRITICAL(&routeMux);
    if (count) amplifyPcm16(packet.data, packet.length, micGainX10);
    if (!count || WiFi.status() != WL_CONNECTED) udpSkipped++;
    else for (int i = 0; i < count; ++i) {
      if (audioUdp.beginPacket(targets[i], UDP_AUDIO_PORT)) {
        size_t written = audioUdp.write(packet.data, packet.length);
        int result = audioUdp.endPacket();
        if (written == packet.length && result == 1) udpSent++;
        else udpErrors++;
      } else udpErrors++;
    }

    if (millis() - lastUdpReport >= 5000) {
      Serial.printf(
          "[AUDIO UDP] sent=%lu errors=%lu skipped=%lu P=%u targets=%d\n",
          (unsigned long)udpSent,
          (unsigned long)udpErrors,
          (unsigned long)udpSkipped,
          (unsigned)button,
          count
      );
      lastUdpReport = millis();
    }

    if (uxQueueMessagesWaiting(audioQueue) > AUDIO_MAX_BACKLOG) {
      nextSend = xTaskGetTickCount();  // naechstes Paket sofort
    } else {
      TickType_t period = pdMS_TO_TICKS(packet.length / 32);  // 16 kHz, 16 Bit
      vTaskDelayUntil(&nextSend, period ? period : 1);
    }
  }
}


// ------------------------------------------------------------
// Setup
// ------------------------------------------------------------


// ------------------------------------------------------------
// MAX98357A – I2S-Lautsprechertest
// ------------------------------------------------------------

#define PIN_SPK_BCLK 14
#define PIN_SPK_LRC  15
#define PIN_SPK_DIN  16
#define PIN_SPK_SD   17

static i2s_chan_handle_t speakerTx = nullptr;

static bool initSpeaker() {
  pinMode(PIN_SPK_SD, OUTPUT);
  digitalWrite(PIN_SPK_SD, LOW);

  i2s_chan_config_t chanCfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);

  esp_err_t err = i2s_new_channel(
      &chanCfg, &speakerTx, nullptr
  );

  if (err != ESP_OK) {
    Serial.printf("[SPEAKER] Channel error: %d\n", err);
    return false;
  }

  i2s_std_config_t stdCfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_STEREO
    ),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)PIN_SPK_BCLK,
      .ws   = (gpio_num_t)PIN_SPK_LRC,
      .dout = (gpio_num_t)PIN_SPK_DIN,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false
      }
    }
  };

  err = i2s_channel_init_std_mode(speakerTx, &stdCfg);

  if (err != ESP_OK) {
    Serial.printf("[SPEAKER] Init error: %d\n", err);
    return false;
  }

  err = i2s_channel_enable(speakerTx);

  if (err != ESP_OK) {
    Serial.printf("[SPEAKER] Enable error: %d\n", err);
    return false;
  }

  digitalWrite(PIN_SPK_SD, HIGH);

  Serial.println("[SPEAKER] I2S initialized");
  return true;
}

static void testSpeaker() {
  if (speakerTx == nullptr) return;

  constexpr int SAMPLE_RATE = 16000;
  constexpr int FRAMES = 256;
  constexpr float FREQUENCY = 440.0f;
  constexpr float AMPLITUDE = 2500.0f;

  int16_t samples[FRAMES * 2];

  for (int offset = 0; offset < SAMPLE_RATE / 2;
       offset += FRAMES) {

    for (int i = 0; i < FRAMES; i++) {
      float phase =
          2.0f * PI * FREQUENCY *
          (offset + i) / SAMPLE_RATE;

      int16_t value =
          (int16_t)(sinf(phase) * AMPLITUDE);

      samples[i * 2]     = value;
      samples[i * 2 + 1] = value;
    }

    size_t written = 0;

    esp_err_t err = i2s_channel_write(
        speakerTx,
        samples,
        sizeof(samples),
        &written,
        pdMS_TO_TICKS(1000)
    );

    if (err != ESP_OK || written != sizeof(samples)) {
      Serial.printf(
          "[SPEAKER] Write error: %d, bytes=%u\n",
          err,
          (unsigned)written
      );
      return;
    }
  }

  digitalWrite(PIN_SPK_SD, LOW);
  Serial.println("[SPEAKER] Amplifier disabled");
  Serial.println("[SPEAKER] Test tone finished");
}


#include "Announcement.h"

// ------------------------------------------------------------
// UDP 5003: PC -> Lautsprecher
// PCM 16 kHz, 16 Bit, Mono, 512 Bytes pro Paket
// ------------------------------------------------------------

#define UDP_SPEAKER_PORT 5003

static WiFiUDP speakerUdp;

static void speakerUdpTask(void* parameter) {
  uint8_t monoBytes[512];
  int16_t stereoSamples[512];

  if (!speakerUdp.begin(UDP_SPEAKER_PORT)) {
    Serial.println("[SPEAKER] UDP receiver start failed");
    vTaskDelete(nullptr);
    return;
  }

  Serial.println("[SPEAKER] UDP receiver started on port 5003");

  bool amplifierEnabled = false;
  uint32_t lastAudio = 0;
  uint32_t handledAnnouncement = 0;

  while (true) {
    uint32_t annGen = annRequestGen;
    if (annGen && annGen != handledAnnouncement) {
      handledAnnouncement = annGen;
      playAnnouncement(annRequestId, annGen);
      annDoneGen = annGen;
      amplifierEnabled = false;
    }
    if (annTestTone.exchange(false)) {
      annPlayTone();
      amplifierEnabled = false;
    }
    uint8_t testId = annTestId.exchange(0);
    if (testId) {
      playAnnouncement(testId, 0, annTestNoSwitch.exchange(false));
      amplifierEnabled = false;
    }

    int packetSize = speakerUdp.parsePacket();

    if (packetSize > 0) {
      if (packetSize != 512 || !isCalledTablet(speakerUdp.remoteIP())) {
        while (speakerUdp.available()) {
          speakerUdp.read();
        }
        Serial.printf(
            "[SPEAKER] Ignored UDP packet: %d bytes\n",
            packetSize
        );
        continue;
      }

      int received = speakerUdp.read(monoBytes, sizeof(monoBytes));

      if (received != 512) {
        continue;
      }

      for (int i = 0; i < 256; i++) {
        uint16_t raw =
            (uint16_t)monoBytes[i * 2] |
            ((uint16_t)monoBytes[i * 2 + 1] << 8);

        int16_t sample = applyGain((int16_t)raw, speakerGainX10);

        stereoSamples[i * 2] = sample;
        stereoSamples[i * 2 + 1] = sample;
      }

      if (speakerTx != nullptr) {
        if (!amplifierEnabled) {
          digitalWrite(PIN_SPK_SD, HIGH);
          amplifierEnabled = true;
          Serial.println("[SPEAKER] Playback started");
        }

        size_t written = 0;

        esp_err_t err = i2s_channel_write(
            speakerTx,
            stereoSamples,
            sizeof(stereoSamples),
            &written,
            pdMS_TO_TICKS(100)
        );

        if (err != ESP_OK || written != sizeof(stereoSamples)) {
          Serial.printf(
              "[SPEAKER] I2S write error: %d, bytes=%u\n",
              err,
              (unsigned)written
          );
        }

        lastAudio = millis();
      }
    }

    if (amplifierEnabled &&
        millis() - lastAudio > 200) {
      digitalWrite(PIN_SPK_SD, LOW);
      amplifierEnabled = false;
      Serial.println("[SPEAKER] Playback stopped");
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(PIN_BUTTON_P1, INPUT_PULLUP);
  pinMode(PIN_BUTTON_P2, INPUT_PULLUP);
  pinMode(PIN_BUTTON_P3, INPUT_PULLUP);

  if (initSpeaker()) {
    testSpeaker();
  }

  Serial.println();
  Serial.println("==============================");
  Serial.println(" ESP32 Open Intercom V2");
  Serial.println(" Doorstation UART + WiFi test");
  Serial.println("==============================");

  Serial.printf(
      "[SYSTEM] PSRAM: %u bytes\n",
      (unsigned)ESP.getPsramSize()
  );

  encBuf = (uint8_t*)ps_malloc(JPEG_MAX + 256);
  decBuf = (uint8_t*)ps_malloc(JPEG_MAX + 256);
  jpegNetBuf = (uint8_t*)ps_malloc(JPEG_MAX);
  jpegLock = xSemaphoreCreateMutex();
  for (int b = 1; b <= 3; ++b) ringSnap[b] = (uint8_t*)ps_malloc(JPEG_MAX);

  if (!encBuf || !decBuf || !jpegNetBuf || !jpegLock) {
    Serial.println("[FATAL] PSRAM buffer allocation failed");

    while (true) {
      delay(1000);
    }
  }

  Serial.printf(
      "[SYSTEM] UART buffers: %u bytes each\n",
      (unsigned)(JPEG_MAX + 256)
  );

  audioQueue = xQueueCreate(
      AUDIO_QUEUE_LENGTH,
      sizeof(AudioPacket)
  );

  if (audioQueue == nullptr) {
    Serial.println("[FATAL] Audio queue allocation failed");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("[AUDIO] Queue initialized");

  /*
   * Start UART first.
   *
   * This deliberately keeps receiving XIAO packets while
   * the WiFi connection is being established.
   */
  xTaskCreatePinnedToCore(
      uartTask,
      "uart-v2",
      4096,
      nullptr,
      2,
      nullptr,
      1
  );

  startWiFi();
  startStationHttp();
  startDiagnostics();
  startOta();
  startAudioSettings();
  startAnnouncements();
  wifiSetupServer.on("/api/missed.jpg", HTTP_GET, [] {
    uint8_t p = wifiSetupServer.arg("p").toInt();
    uint32_t call = (uint32_t)strtoul(wifiSetupServer.arg("call").c_str(), nullptr, 10);
    if (!otaLocalClient()) { wifiSetupServer.send(403, "text/plain", "Nur im lokalen Netz"); return; }
    if (p < 1 || p > 3 || !call || ringSnapCall[p] != call || !ringSnapLen[p]) {
      wifiSetupServer.send(404, "text/plain", "Kein Bild");
      return;
    }
    wifiSetupServer.sendHeader("Cache-Control", "no-store");
    wifiSetupServer.send_P(200, "image/jpeg", (const char*)ringSnap[p], ringSnapLen[p]);
  });
  discoveryUdp.begin(5004);

  xTaskCreatePinnedToCore(
      videoTask,
      "video-v2",
      6144,
      nullptr,
      1,
      nullptr,
      0
  );

  xTaskCreatePinnedToCore(
      controlTask,
      "control-v2",
      4096,
      nullptr,
      1,
      nullptr,
      0
  );

  xTaskCreatePinnedToCore(
      audioUdpTask,
      "audio-udp-v2",
      4096,
      nullptr,
      1,
      nullptr,
      0
  );


  xTaskCreatePinnedToCore(
      speakerUdpTask,
      "speaker-udp-v2",
      12288,  // Ansage: LittleFS + I2S-Umschaltung
      nullptr,
      1,
      nullptr,
      0
  );

  lastReport = millis();
  lastWiFiCheck = millis();

  Serial.println("[SYSTEM] READY");
}


// ------------------------------------------------------------
// Main loop
// ------------------------------------------------------------

void loop() {
  handleOta();
  handleWifiMaintenance();
  // Wi-Fi/AP changes run on the main loop, not on the control socket task.
  bool startRequestedSetup = false;
  portENTER_CRITICAL(&stateMux);
  if (wifiSetupRequested) {
    startRequestedSetup = intercomState == IntercomState::IDLE;
    wifiSetupRequested = false;
  }
  portEXIT_CRITICAL(&stateMux);
  if (startRequestedSetup) startWifiSetup();
  handleWifiSetup();
  handleStationDiscovery();
  uint32_t now = millis();
  // Abgelaufene Auswahl (Timer) einmal pro Sekunde ausschalten.
  static uint32_t lastAnnCheck = 0;
  if (now - lastAnnCheck >= 1000) { lastAnnCheck = now; annCheckExpiry(); }
  // Klingeln: gewaehlter Satz der Klingel nach der Wartezeit, im Sofort-Modus
  // nach 1 s (Tablets klingeln dann nicht). Der Ruf bleibt annehmbar.
  // Nach Ansage und Wartezeit: IDLE und "verpasst".
  AnnFloor annCfg[4] = {};
  AnnEntry annSel[4] = {};
  bool annOn[4] = {};
  for (uint8_t b = 1; b <= 3; ++b) annOn[b] = annActive(b, annCfg[b], annSel[b]);
  static uint32_t annStartedAt = 0, annDurMs = 0;  // Anforderung (ms seit Klingeln), Satzlaenge
  bool expired = false, announce = false;
  uint8_t expiredButton = 0, snapButton = 0, announceButton = 0;
  uint32_t expiredCall = 0, snapCall = 0;
  uint32_t waitMs = ringWaitMs;
  portENTER_CRITICAL(&stateMux);
  bool ringing = intercomState == IntercomState::RINGING_P1 ||
                 intercomState == IntercomState::RINGING_P2 ||
                 intercomState == IntercomState::RINGING_P3;
  if (ringing && (uint32_t)(now - ringingStartedAt) >= RING_SNAPSHOT_DELAY_MS) {
    snapButton = activeButton;
    snapCall = stateGeneration;
  }
  if (ringing && activeButton >= 1 && activeButton <= 3) {
    uint32_t elapsed = now - ringingStartedAt;
    uint32_t gen = stateGeneration;
    uint8_t b = activeButton;
    if (annRequestGen != gen) {
      uint32_t startAt = annOn[b] && annCfg[b].instant ? ANN_INSTANT_DELAY_MS : waitMs;
      if (annOn[b] && elapsed >= startAt) {
        annRequestId = annCfg[b].sel;
        annRequestGen = gen;
        annStartedAt = elapsed;
        annDurMs = annDurationMs(annSel[b]);
        announce = true;
        announceButton = b;
      } else if (!annOn[b] && elapsed >= waitMs) {
        expired = true;
      }
    } else {
      uint32_t maxEnd = annStartedAt + annDurMs + 5000;
      if ((annDoneGen == gen || elapsed >= maxEnd) && elapsed >= waitMs) expired = true;
    }
    if (expired) {
      expiredButton = b;
      expiredCall = gen;
      intercomState = IntercomState::IDLE;
      activeButton = 0;
      stateGeneration++;
    }
  }
  portEXIT_CRITICAL(&stateMux);
  if (announce) Serial.printf("[STATE] Announcement P%u: Satz %u%s\n", announceButton,
                              annCfg[announceButton].sel, annCfg[announceButton].instant ? " (sofort)" : "");
  if (snapButton && !expired) captureRingSnapshot(snapButton, snapCall);
  if (expired) {
    missedButton = expiredButton;
    missedCall = expiredCall;
    missedCount++;
    Serial.printf("[STATE] Unanswered call P%u ended (missed)\n", expiredButton);
  }

  static int lastP1 = HIGH;
  static int lastP2 = HIGH;
  static int lastP3 = HIGH;

  int p1 = digitalRead(PIN_BUTTON_P1);
  int p2 = digitalRead(PIN_BUTTON_P2);
  int p3 = digitalRead(PIN_BUTTON_P3);
  // Suppress the selection press (including release) until all buttons are up.
  if (!bootButtonsArmed) {
    if (p1 == HIGH && p2 == HIGH && p3 == HIGH) bootButtonsArmed = true;
    p1 = p2 = p3 = HIGH;
  }


  // Alle Klingeltasten loesen beim Druecken aus. Die WLAN-Einrichtung oeffnet
  // sich ueber das EG-Tablet oder automatisch nach 60 s ohne WLAN.
  bool p1Pressed = (lastP1 == HIGH && p1 == LOW);
  bool p2Pressed = (lastP2 == HIGH && p2 == LOW);
  bool p3Pressed = (lastP3 == HIGH && p3 == LOW);

  lastP1 = p1;
  lastP2 = p2;
  lastP3 = p3;

  if (p1Pressed) {
    if (changeIntercomState(
            IntercomState::IDLE,
            IntercomState::RINGING_P1)) {
      Serial.println("[STATE] IDLE -> RINGING_P1");
    }
  } else if (p2Pressed) {
    if (changeIntercomState(
            IntercomState::IDLE,
            IntercomState::RINGING_P2)) {
      Serial.println("[STATE] IDLE -> RINGING_P2");
    }
  } else if (p3Pressed) {
    if (changeIntercomState(
            IntercomState::IDLE,
            IntercomState::RINGING_P3)) {
      Serial.println("[STATE] IDLE -> RINGING_P3");
    }
  }

  if (now - lastWiFiCheck >= 5000) {
    checkWiFi();
    lastWiFiCheck = now;
  }
  handleWifiRoaming();


  if (now - lastReport >= 5000) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WIFI RSSI] %d dBm | Kanal %d | IP %s\n",
                    WiFi.RSSI(), WiFi.channel(), WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[WIFI RSSI] nicht verbunden");
    }

    Serial.printf("[AUDIO] RX packets=%lu bytes=%lu dropped=%lu\n",
                  (unsigned long)audioPackets,
                  (unsigned long)audioBytes,
                  (unsigned long)droppedPackets);
    lastReport = now;
  }

  delay(10);
}
