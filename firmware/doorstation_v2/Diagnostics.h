#pragma once
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_ota_ops.h>
static const char* resetReasonName() {
  switch(esp_reset_reason()) {
    case ESP_RST_POWERON:return "Einschalten / Versorgung";
    case ESP_RST_SW:return "Software-Neustart";
    case ESP_RST_PANIC:return "Software-Absturz (Panic)";
    case ESP_RST_INT_WDT:return "Interrupt-Watchdog";
    case ESP_RST_TASK_WDT:return "Task-Watchdog";
    case ESP_RST_WDT:return "Watchdog";
    case ESP_RST_BROWNOUT:return "Unterspannung (Brownout)";
    case ESP_RST_EXT:return "Externer Reset";
    case ESP_RST_DEEPSLEEP:return "Aufwachen aus Tiefschlaf";
    default:return "Sonstiger / unbekannter Reset";
  }
}
// Read-only local diagnostics. No credentials or configuration operations.
static void diagnosticsJson() {
  TabletRoute routes[MAX_TABLETS];
  portENTER_CRITICAL(&routeMux);
  memcpy(routes, tabletRoutes, sizeof(routes));
  portEXIT_CRITICAL(&routeMux);
  uint32_t last, generation;
  size_t length;
  portENTER_CRITICAL(&jpegMux);
  last = lastJpegAt; generation = jpegNetGeneration; length = jpegNetLen;
  portEXIT_CRITICAL(&jpegMux);
  bool connected = WiFi.status() == WL_CONNECTED;
  wifi_ps_type_t powerSave = WIFI_PS_NONE;
  esp_err_t powerSaveResult = esp_wifi_get_ps(&powerSave);
  const char* powerSaveName = powerSaveResult != ESP_OK ? "unknown" :
      powerSave == WIFI_PS_NONE ? "NONE" :
      powerSave == WIFI_PS_MIN_MODEM ? "MIN_MODEM" :
      powerSave == WIFI_PS_MAX_MODEM ? "MAX_MODEM" : "unknown";
  StateSnapshot state = getStateSnapshot();
  String body; body.reserve(1800);
  body = "{\"uptime_ms\":" + String(millis()) + ",\"connected\":" + (connected ? "true" : "false");
  body += ",\"ssid\":" + jsonQuote(WiFi.SSID()) + ",\"ip\":" + jsonQuote(WiFi.localIP().toString());
  body += ",\"rssi_dbm\":" + (connected ? String(WiFi.RSSI()) : String("null"));
  body += ",\"channel\":" + String(WiFi.channel()) + ",\"bssid\":" + jsonQuote(WiFi.BSSIDstr());
  body += ",\"wifi_roams\":" + String(wifiRoamCount);
  body += ",\"state\":" + jsonQuote(intercomStateName(state.state)) + ",\"active_button\":" + String(state.button);
  body += ",\"uart_bytes\":" + String(uartBytes.load()) + ",\"jpeg_packets\":" + String(jpegPackets.load());
  body += ",\"jpeg_bytes\":" + String(jpegBytes.load()) + ",\"last_jpeg_size\":" + String(length);
  body += ",\"last_jpeg_age_ms\":" + (generation ? String((uint32_t)(millis()-last)) : String("null"));
  body += ",\"audio_packets\":" + String(audioPackets.load()) + ",\"audio_bytes\":" + String(audioBytes.load());
  body += ",\"dropped_packets\":" + String(droppedPackets.load()) + ",\"dropped_audio_queue\":" + String(droppedAudioQueue.load()) + ",\"audio_underruns\":" + String(audioUnderruns.load()) + ",\"uart_overflows\":" + String(uartOverflows.load()) + ",\"uart_frame_errors\":" + String(uartFrameErrors.load()) + ",\"free_heap\":" + String(ESP.getFreeHeap());
  body += ",\"reset_reason\":" + jsonQuote(resetReasonName()) + ",\"reset_code\":" + String((int)esp_reset_reason());
  body += ",\"firmware_slot\":" + jsonQuote(esp_ota_get_running_partition()->label);
  body += ",\"firmware_id\":\"812524a-bootwifi-20261030\"";
  body += ",\"firmware_build\":" + jsonQuote(String(__DATE__) + " " + __TIME__);
  body += ",\"wifi_ps_mode\":" + jsonQuote(powerSaveName);
  body += ",\"wifi_ps_read_error\":" + String((int)powerSaveResult);
  body += ",\"mic_gain_x10\":" + String(micGainX10.load()) + ",\"speaker_gain_x10\":" + String(speakerGainX10.load());
  body += ",\"wifi_sleep_disable_accepted\":" + String(wifiSleepDisableAccepted ? "true" : "false");
  body += ",\"tablets\":[";
  bool first = true;
  for (int i=0;i<MAX_TABLETS;i++) if (routes[i].connected) {
    if (!first) body += ','; first = false;
    body += "{\"ip\":" + jsonQuote(routes[i].ip.toString()) + ",\"button\":" + String(routes[i].button) + "}";
  }
  body += "]}";
  wifiSetupServer.sendHeader("Cache-Control", "no-store");
  wifiSetupServer.send(200, "application/json", body);
}
static void startDiagnostics() {
  wifiSetupServer.on("/api/diagnose", HTTP_GET, diagnosticsJson);
  wifiSetupServer.on("/diagnose", HTTP_GET, [] {
    wifiSetupServer.sendHeader("Cache-Control", "no-store");
    String html = R"HTML(<!doctype html>
<html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Türstation – Diagnose</title><style>
body{font:17px system-ui,sans-serif;background:#101820;color:#edf3f6;max-width:860px;margin:auto;padding:24px}h1{font-size:28px}section{background:#1c2a35;padding:18px;border-radius:12px;margin:16px 0}dl{display:grid;grid-template-columns:1fr 1fr;gap:12px}dt{color:#b5c6d2}dd{margin:0;overflow-wrap:anywhere}#status{color:#7edbc1}.note{color:#b5c6d2;font-size:14px}li{margin:8px 0}</style>
<h1>Türstation · Diagnose</h1><p><a href="/update" style="color:#7edbc1">Firmware-Update</a> · <a href="/audio" style="color:#7edbc1">Lautstärke</a> · <a href="/ansage" style="color:#7edbc1">Ansagen</a></p><p id="status" role="status">Verbindung wird geprüft …</p>
<section><h2>WLAN und Betrieb</h2><dl id="network"></dl></section>
<section><h2>Kamera und Audio</h2><dl id="data"></dl><p class="note">Zähler seit Stationsstart. Verworfen umfasst ungültige UART-Pakete und volle Audio-Warteschlangen. Ein empfangenes Kamerapaket bestätigt noch keine gute Bildqualität.</p></section>
<section><h2>Verbundene Tablets</h2><ul id="tablets"></ul></section>
<p class="note">Automatische Aktualisierung alle 2 Sekunden. Diese Seite liest nur Statusdaten.</p>
<script>
const $=id=>document.getElementById(id);let previous=null;
function rows(id,items){$(id).replaceChildren();for(const [key,value] of items){const k=document.createElement('dt'),v=document.createElement('dd');k.textContent=key;v.textContent=value;$(id).append(k,v)}}
const num=n=>n.toLocaleString('de-DE');
async function update(){try{const r=await fetch('/api/diagnose',{cache:'no-store',signal:AbortSignal.timeout(5000)});if(!r.ok)throw Error(r.status);const d=await r.json();
$('status').textContent='Aktuell · '+new Date().toLocaleTimeString('de-DE');$('status').style.color='#7edbc1';
let rate='Wird ermittelt';if(previous&&d.uptime_ms>previous.uptime_ms&&d.jpeg_packets>=previous.jpeg_packets)rate=((d.jpeg_packets-previous.jpeg_packets)*1000/(d.uptime_ms-previous.uptime_ms)).toFixed(1)+' Bilder/s';
rows('network',[['WLAN',d.connected?d.ssid:'Nicht verbunden'],['IP-Adresse',d.ip],['Signal',d.rssi_dbm===null?'–':d.rssi_dbm+' dBm'],['Kanal / Zugangspunkt',d.channel+' / '+d.bssid],['Repeater-Wechsel',num(d.wifi_roams)],['Neustartgrund',d.reset_reason],['Firmware',d.firmware_id],['Build',d.firmware_build],['WLAN-Energiesparen',d.wifi_ps_mode==='NONE'?'Aus':d.wifi_ps_mode==='unknown'?'Nicht lesbar (Fehler '+d.wifi_ps_read_error+')':'An · '+d.wifi_ps_mode],['Abschalten bestätigt',d.wifi_sleep_disable_accepted?'Ja':'Nein'],['Firmware-Platz',d.firmware_slot],['Laufzeit',Math.floor(d.uptime_ms/1000)+' Sekunden'],['Zustand',d.state+(d.active_button?' · P'+d.active_button:'')],['Freier Speicher',num(d.free_heap)+' Bytes']]);
rows('data',[['UART empfangen',num(d.uart_bytes)+' Bytes'],['Kamerapakete',num(d.jpeg_packets)],['Bildrate',rate],['Letztes Bild',d.last_jpeg_age_ms===null?'Noch keines':(d.last_jpeg_age_ms/1000).toFixed(1)+' s alt · '+num(d.last_jpeg_size)+' Bytes'],['Audiopakete',num(d.audio_packets)],['Audio empfangen',num(d.audio_bytes)+' Bytes'],['Verworfen',num(d.dropped_packets)+' (davon Ton-Warteschlange voll: '+num(d.dropped_audio_queue)+')'],['Ton-Unterbrechungen',num(d.audio_underruns)],['UART-Überlauf / Übertragungsfehler',num(d.uart_overflows)+' / '+num(d.uart_frame_errors)]]);
$('tablets').replaceChildren();for(const t of d.tablets){const li=document.createElement('li');li.textContent=(t.button?'P'+t.button:'Noch nicht zugeordnet')+' · '+t.ip;$('tablets').append(li)}if(!d.tablets.length){const li=document.createElement('li');li.textContent='Keine Steuerverbindung';$('tablets').append(li)}previous=d;
}catch(e){$('status').textContent='Keine aktuelle Antwort – angezeigte Werte können veraltet sein';$('status').style.color='#ffba82'}finally{setTimeout(update,2000)}}update();
</script></html>)HTML";
    wifiSetupServer.setContentLength(html.length());
    wifiSetupServer.send(200, "text/html; charset=utf-8", "");
    for (size_t offset=0; offset<html.length(); offset+=512) {
      wifiSetupServer.sendContent(html.substring(offset, offset+512));
    }
  });
}
