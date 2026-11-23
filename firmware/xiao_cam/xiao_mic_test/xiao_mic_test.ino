#include <Arduino.h>
#include <ESP_I2S.h>

I2SClass I2S;

void setup()
{
  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("[BOOT] XIAO ESP32-S3 Sense microphone test");

  // XIAO ESP32-S3 Sense
  // GPIO42 = PDM CLK
  // GPIO41 = PDM DATA
  I2S.setPinsPdmRx(42, 41);

  if (!I2S.begin(
        I2S_MODE_PDM_RX,
        16000,
        I2S_DATA_BIT_WIDTH_16BIT,
        I2S_SLOT_MODE_MONO)) {

    Serial.println("[MIC] I2S initialization FAILED");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("[MIC] PDM microphone initialized");
  Serial.println("[MIC] Speak or tap near microphone...");
}

void loop()
{
  static uint32_t lastPrint = 0;
  static int32_t peak = 0;

  int sample = I2S.read();

  if (sample != -1) {
    int16_t s = (int16_t)sample;

    int32_t level = abs((int32_t)s);

    if (level > peak) {
      peak = level;
    }
  }

  if (millis() - lastPrint >= 100) {
    lastPrint = millis();

    Serial.printf("[MIC] peak=%ld\n", (long)peak);
    peak = 0;
  }
}