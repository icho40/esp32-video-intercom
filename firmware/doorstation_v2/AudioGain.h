#pragma once
// Einstellbare Lautstaerke der Tuerstation (Admin-Webseite /audio).
// Mikrofon: Anhebung des Tuerton fuer Tablets/Handys. Lautsprecher: Anhebung
// der Tablet-Stimme am Tuerlautsprecher. Werte in Zehnteln (10 = 1,0-fach),
// gespeichert im NVS-Namensraum "intercom-audio", wirken sofort.
#include <Preferences.h>
#include <atomic>
#include <math.h>

static constexpr int32_t GAIN_MIN_X10 = 5;    // 0,5-fach
static constexpr int32_t GAIN_MAX_X10 = 80;   // 8-fach
static constexpr int32_t MIC_GAIN_DEFAULT_X10 = 40;      // Tuerton vorher zu leise
static constexpr int32_t SPEAKER_GAIN_DEFAULT_X10 = 10;  // unveraendert wie bisher

static std::atomic<int32_t> micGainX10{MIC_GAIN_DEFAULT_X10};
static std::atomic<int32_t> speakerGainX10{SPEAKER_GAIN_DEFAULT_X10};

static int32_t clampGain(long value) {
  if (value < GAIN_MIN_X10) return GAIN_MIN_X10;
  if (value > GAIN_MAX_X10) return GAIN_MAX_X10;
  return (int32_t)value;
}

static inline int16_t applyGain(int16_t sample, int32_t gainX10) {
  int32_t v = (int32_t)sample * gainX10 / 10;
  if (v > INT16_MAX) v = INT16_MAX;
  else if (v < INT16_MIN) v = INT16_MIN;
  return (int16_t)v;
}

// PCM16 little endian an Ort und Stelle verstaerken.
static void amplifyPcm16(uint8_t* data, size_t length, int32_t gainX10) {
  if (gainX10 == 10) return;
  for (size_t i = 0; i + 1 < length; i += 2) {
    int16_t s = (int16_t)((uint16_t)data[i] | ((uint16_t)data[i + 1] << 8));
    uint16_t v = (uint16_t)applyGain(s, gainX10);
    data[i] = (uint8_t)(v & 0xFF);
    data[i + 1] = (uint8_t)(v >> 8);
  }
}

static void loadAudioGains() {
  Preferences prefs;
  if (prefs.begin("intercom-audio", true)) {
    micGainX10 = clampGain(prefs.getInt("mic", MIC_GAIN_DEFAULT_X10));
    speakerGainX10 = clampGain(prefs.getInt("spk", SPEAKER_GAIN_DEFAULT_X10));
    prefs.end();
  }
  Serial.printf("[AUDIO GAIN] mic=%.1f speaker=%.1f\n", micGainX10 / 10.0, speakerGainX10 / 10.0);
}

static bool saveAudioGains(int32_t mic, int32_t spk) {
  Preferences prefs;
  if (!prefs.begin("intercom-audio", false)) return false;
  bool ok = prefs.putInt("mic", mic) && prefs.putInt("spk", spk);
  prefs.end();
  return ok;
}

static String gainLabel(int32_t x10) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f-fach (%+.0f dB)", x10 / 10.0, 20.0 * log10(x10 / 10.0));
  String s(buf);
  s.replace(".", ",");
  return s;
}

// Benoetigt OtaUpdate.h (otaAuthorized, otaReject, otaToken).
static void startAudioSettings() {
  loadAudioGains();
  wifiSetupServer.on("/audio", HTTP_GET, [] {
    if (!otaAuthorized()) { otaReject(); return; }
    int32_t mic = micGainX10, spk = speakerGainX10;
    String html = R"HTML(<!doctype html><html lang="de"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Lautstärke</title>
<style>body{font-family:system-ui,sans-serif;background:#111318;color:#e8ecf2;max-width:560px;margin:24px auto;padding:0 16px}
a{color:#7edbc1}label{display:block;margin:22px 0 6px;font-weight:600}input[type=range]{width:100%}
output{color:#7edbc1}.note{color:#b5c6d2;font-size:14px}button{margin-top:24px;padding:10px 18px;font-size:16px}</style>
<h1>Türstation · Lautstärke</h1><p><a href="/diagnose">Zurück zur Diagnose</a> · <a href="/ansage">Ansagen bei Abwesenheit</a></p>%OK%
<form method="post" action="/audio">
<label>Türton am Tablet/Handy (Mikrofon der Türstation)</label>
<input type="range" name="mic" min="5" max="80" step="5" value="%MIC%" oninput="m.value=f(this.value)">
<output id="m">%MICL%</output>
<label>Stimme an der Tür (Lautsprecher der Türstation)</label>
<input type="range" name="spk" min="5" max="80" step="5" value="%SPK%" oninput="s.value=f(this.value)">
<output id="s">%SPKL%</output>
<input type="hidden" name="token" value="%TOKEN%">
<p class="note">1,0-fach = unverändert. Zu hoch eingestellt rauscht oder verzerrt es; am Testplatz kann es eher pfeifen.
Die Werte gelten sofort nach dem Speichern und bleiben nach einem Neustart erhalten.</p>
<button type="submit">Speichern</button></form>
<script>function f(v){v=v/10;return v.toFixed(1).replace('.',',')+'-fach ('+(v>=1?'+':'')+Math.round(20*Math.log10(v))+' dB)'}</script></html>)HTML";
    html.replace("%OK%", wifiSetupServer.hasArg("ok") ? "<p style=\"color:#7edbc1\">Gespeichert.</p>" : "");
    html.replace("%MIC%", String(mic));
    html.replace("%SPK%", String(spk));
    html.replace("%MICL%", gainLabel(mic));
    html.replace("%SPKL%", gainLabel(spk));
    html.replace("%TOKEN%", otaToken);
    wifiSetupServer.sendHeader("Cache-Control", "no-store");
    wifiSetupServer.send(200, "text/html; charset=utf-8", html);
  });
  wifiSetupServer.on("/audio", HTTP_POST, [] {
    if (!otaAuthorized()) { otaReject(); return; }
    if (wifiSetupServer.arg("token") != otaToken ||
        !wifiSetupServer.hasArg("mic") || !wifiSetupServer.hasArg("spk")) {
      wifiSetupServer.send(400, "text/plain; charset=utf-8", "Ungültige Anfrage. Seite neu laden.");
      return;
    }
    int32_t mic = clampGain(wifiSetupServer.arg("mic").toInt());
    int32_t spk = clampGain(wifiSetupServer.arg("spk").toInt());
    micGainX10 = mic;
    speakerGainX10 = spk;
    bool saved = saveAudioGains(mic, spk);
    Serial.printf("[AUDIO GAIN] set mic=%.1f speaker=%.1f saved=%d\n", mic / 10.0, spk / 10.0, saved);
    wifiSetupServer.sendHeader("Location", "/audio?ok=1");
    wifiSetupServer.send(303, "text/plain", "");
  });
}
