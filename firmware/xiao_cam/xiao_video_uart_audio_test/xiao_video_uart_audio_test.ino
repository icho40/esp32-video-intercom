#include <Arduino.h>
#include "esp_camera.h"
#include <HardwareSerial.h>
#include "esp_log.h"
#define CAMERA_MODEL_XIAO_ESP32S3
#include "camera_pins.h"
#include <ESP_I2S.h>

I2SClass I2S;
HardwareSerial VideoUART(1);

#define VIDEO_TX    43
#define VIDEO_BAUD  1000000

typedef struct __attribute__((packed)) {
  uint8_t  type;
  uint16_t seq;
  uint32_t ts_ms;
  uint32_t payload_len;
} pkt_hdr_t;

static uint16_t seq = 0;
static SemaphoreHandle_t uartMutex = nullptr;

static size_t cobs_encode(const uint8_t* in, size_t len, uint8_t* out) {
  const uint8_t* end = in + len;
  uint8_t* start = out;
  uint8_t* code_ptr = out++;
  uint8_t code = 1;

  while (in < end) {
    if (*in == 0) {
      *code_ptr = code;
      code_ptr = out++;
      code = 1;
      in++;
    } else {
      *out++ = *in++;
      code++;
      if (code == 0xFF) {
        *code_ptr = code;
        code_ptr = out++;
        code = 1;
      }
    }
  }

  *code_ptr = code;
  return out - start;
}

static void sendAudioPacket(const int16_t* samples, size_t sampleCount) {
  pkt_hdr_t hdr;
  hdr.type = 0x02;
  hdr.seq = 0;
  hdr.ts_ms = millis();
  hdr.payload_len = sampleCount * sizeof(int16_t);

  const size_t rawLen = sizeof(hdr) + hdr.payload_len;
  const size_t encMax = rawLen + rawLen / 254 + 2;

  uint8_t* raw = (uint8_t*)ps_malloc(rawLen);
  uint8_t* enc = (uint8_t*)ps_malloc(encMax);

  if (!raw || !enc) {
    if (raw) free(raw);
    if (enc) free(enc);
    Serial.println("[AUDIO] alloc failed");
    return;
  }

  memcpy(raw, &hdr, sizeof(hdr));
  memcpy(raw + sizeof(hdr), samples, hdr.payload_len);

  size_t encLen = cobs_encode(raw, rawLen, enc);

  xSemaphoreTake(uartMutex, portMAX_DELAY);
  hdr.seq = 0;
  memcpy(raw, &hdr, sizeof(hdr));
  encLen = cobs_encode(raw, rawLen, enc);
  size_t written = VideoUART.write(enc, encLen);
  written += VideoUART.write((uint8_t)0x00);
  xSemaphoreGive(uartMutex);

  static uint32_t audioTxPackets = 0;
  static uint32_t audioTxErrors = 0;
  static uint32_t lastAudioTxReport = 0;

  audioTxPackets++;
  if (written != encLen + 1) audioTxErrors++;

  uint32_t now = millis();
  if (now - lastAudioTxReport >= 5000) {
    Serial.printf(
        "[AUDIO TX] packets=%lu errors=%lu last_write=%u/%u\n",
        (unsigned long)audioTxPackets,
        (unsigned long)audioTxErrors,
        (unsigned)written,
        (unsigned)(encLen + 1)
    );
    lastAudioTxReport = now;
  }

  free(raw);
  free(enc);
}


static void microphoneTask(void* parameter) {
  for (;;) {
// ===================== Mikrofon + Audio UART =====================
static uint32_t lastMicPrint = 0;
static int32_t micPeak = 0;
static uint32_t micSamples = 0;

static const size_t AUDIO_SAMPLES = 256;
static int16_t audioBuf[AUDIO_SAMPLES];
static size_t audioPos = 0;

for (int i = 0; i < 1024; i++) {
  int sample = I2S.read();

  if (sample != -1) {
    int16_t s = (int16_t)sample;
    int32_t level = abs((int32_t)s);

    micSamples++;

    if (level > micPeak) {
      micPeak = level;
    }

    audioBuf[audioPos++] = s;

    if (audioPos >= AUDIO_SAMPLES) {
      sendAudioPacket(audioBuf, AUDIO_SAMPLES);
      audioPos = 0;
    }
  }
}

if (millis() - lastMicPrint >= 1000) {
  lastMicPrint = millis();

  Serial.printf("[MIC] peak=%ld samples=%u/s\n",
                (long)micPeak,
                (unsigned)micSamples);

  micPeak = 0;
  micSamples = 0;
}

    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("[BOOT] setup started");

  Serial.printf("[PSRAM] found=%s\n",
              psramFound() ? "YES" : "NO");

Serial.printf("[PSRAM] size=%u bytes\n",
              ESP.getPsramSize());

if (!psramFound()) {
  Serial.println("!!! FATAL: PSRAM NICHT AKTIV !!!");
  Serial.println("Arduino IDE: Werkzeuge -> PSRAM -> OPI PSRAM");
  while (true) {
    delay(1000);
  }
}
  Serial.println();
  Serial.println("[BOOT] XIAO JPEG UART test");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QQVGA;
  config.jpeg_quality = 12;
  config.fb_count     = 2;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.xclk_freq_hz = 10000000;
// Ausführliche ESP-IDF-Debugausgabe für Kamera
esp_log_level_set("*", ESP_LOG_INFO);
esp_log_level_set("camera", ESP_LOG_NONE);
esp_log_level_set("cam_hal", ESP_LOG_NONE);
esp_log_level_set("sccb", ESP_LOG_NONE);
esp_log_level_set("sensor", ESP_LOG_NONE);

Serial.println("[CAM] detailed logging enabled");
delay(100);



  Serial.println("[CAM] before init");
  esp_err_t err = esp_camera_init(&config);
  Serial.printf("[CAM] init err=0x%x\n", err);

  if (err != ESP_OK) {
    Serial.println("[CAM] init failed, stopped");
    while (true) delay(1000);
  }

  Serial.println("[CAM] init OK");


// ===================== PDM Mikrofon =====================
// XIAO ESP32-S3 Sense:
// GPIO42 = PDM CLK
// GPIO41 = PDM DATA

I2S.setPinsPdmRx(42, 41);

if (!I2S.begin(
      I2S_MODE_PDM_RX,
      16000,
      I2S_DATA_BIT_WIDTH_16BIT,
      I2S_SLOT_MODE_MONO)) {

  Serial.println("[MIC] PDM initialization FAILED");
  while (true) {
    delay(1000);
  }
}

Serial.println("[MIC] PDM initialized: 16 kHz / 16 bit / mono");

  VideoUART.begin(VIDEO_BAUD, SERIAL_8N1, -1, VIDEO_TX);
  Serial.println("[UART] VideoUART started");
  uartMutex = xSemaphoreCreateMutex();
  if (!uartMutex) {
    Serial.println("[FATAL] UART mutex failed");
    while (true) delay(1000);
  }

  BaseType_t result = xTaskCreatePinnedToCore(
      microphoneTask, "Microphone", 8192, nullptr, 1, nullptr, 0);

  if (result != pdPASS) {
    Serial.println("[FATAL] Microphone task failed");
    while (true) delay(1000);
  }

  Serial.println("[MIC] FreeRTOS task started");
}

void loop() {
  static uint32_t lastLoopPrint = 0;
  if (millis() - lastLoopPrint >= 1000) {
    lastLoopPrint = millis();
    Serial.println("[LOOP] alive");
  }

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    static uint32_t lastCamError = 0;
    if (millis() - lastCamError >= 1000) {
      lastCamError = millis();
      Serial.println("[CAM] fb failed");
    }
  } else {

  static uint32_t okCnt = 0;
  if ((okCnt++ % 30) == 0) {
    Serial.printf("[CAM] frame OK len=%u w=%u h=%u format=%u\n",
                  fb->len, fb->width, fb->height, fb->format);
  }

  pkt_hdr_t hdr;
  hdr.type = 0x01;
  hdr.seq = 0;
  hdr.ts_ms = millis();
  hdr.payload_len = fb->len;

  const size_t rawLen = sizeof(hdr) + fb->len;
  const size_t encMax = rawLen + rawLen / 254 + 2;

  uint8_t* raw = (uint8_t*)ps_malloc(rawLen);
  uint8_t* enc = (uint8_t*)ps_malloc(encMax);

  if (raw && enc) {
    memcpy(raw, &hdr, sizeof(hdr));
    memcpy(raw + sizeof(hdr), fb->buf, fb->len);

    xSemaphoreTake(uartMutex, portMAX_DELAY);
    hdr.seq = seq++;
    memcpy(raw, &hdr, sizeof(hdr));
    size_t encLen = cobs_encode(raw, rawLen, enc);
    VideoUART.write(enc, encLen);
    VideoUART.write((uint8_t)0x00);
    xSemaphoreGive(uartMutex);
  } else {
    Serial.println("[ERR] alloc failed");
  }

  if (raw) free(raw);
  if (enc) free(enc);

  esp_camera_fb_return(fb);
  }


delay(1);

}
