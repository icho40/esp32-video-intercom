#pragma once
#include <Update.h>
#include <esp_ota_ops.h>
static String otaPassword, otaToken;
static bool otaWriting=false, otaSucceeded=false, otaDenied=false;
static uint32_t otaLastData=0, otaRestartAt=0;
static String randomOtaSecret() {
  String value;
  for(int i=0;i<4;i++) { char part[9]; snprintf(part,sizeof(part),"%08lx",(unsigned long)esp_random()); value+=part; }
  return value;
}
// Only the station interface on HOME_WIFI_SSID and clients in its IPv4 subnet.
static bool otaLocalClient() {
  if(WiFi.status()!=WL_CONNECTED || WiFi.SSID()!=HOME_WIFI_SSID)return false;
  auto client=wifiSetupServer.client();
  uint32_t mask=(uint32_t)WiFi.subnetMask();
  return client.localIP()==WiFi.localIP() &&
      (((uint32_t)client.remoteIP() & mask)==((uint32_t)WiFi.localIP() & mask));
}
static bool otaAuthorized() {
  return otaLocalClient() && otaPassword.length() && wifiSetupServer.authenticate("admin",otaPassword.c_str());
}
static void otaReject() {
  if(!otaLocalClient() || !otaPassword.length())wifiSetupServer.send(403,"text/plain","OTA nur im lokalen " HOME_WIFI_SSID " verfuegbar");
  else wifiSetupServer.requestAuthentication(DIGEST_AUTH,"Doorstation update");
}
static void printOtaAccess() {
  if(otaPassword.length())Serial.printf("OTA_ACCESS user=admin password=%s\n",otaPassword.c_str());
  else Serial.println("OTA_UNAVAILABLE");
}
// USB only: 8-64 printable ASCII characters without spaces. Not allowed during an upload.
static bool setOtaPassword(const String& password) {
  if(otaWriting || password.length()<8 || password.length()>64)return false;
  for(size_t i=0;i<password.length();i++) if(password[i]<0x21 || password[i]>0x7e)return false;
  Preferences prefs;
  if(!prefs.begin("intercom-ota",false))return false;
  bool ok=prefs.putString("password",password)==password.length();
  prefs.end();
  if(ok)otaPassword=password;
  return ok;
}
static void startOta() {
  Preferences prefs;
  if(prefs.begin("intercom-ota",false)) {
    otaPassword=prefs.getString("password","");
    if(!otaPassword.length()) {
      otaPassword=randomOtaSecret();
      if(!prefs.putString("password",otaPassword))otaPassword="";
    }
    prefs.end();
  }
  otaToken=randomOtaSecret();
  wifiSetupServer.on("/update",HTTP_GET,[] {
    if(!otaAuthorized()){otaReject();return;}
    wifiSetupServer.sendHeader("Cache-Control","no-store");
    String html="<!doctype html><html lang=de><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Firmware-Update</title><h1>Tu erstation aktualisieren</h1><p>Nur die passende Firmware-Datei (.ino.bin), kein Gesamt-Flash-Abbild. Stromversorgung eingeschaltet lassen. Nach Erfolg startet die Station neu.</p><form method=post action='/update?token="+otaToken+"' enctype=multipart/form-data><input type=file name=firmware accept='.bin' required><button>Firmware installieren</button></form><p><a href=/diagnose>Zur Diagnose</a></p></html>";
    html.replace("Tu erstation","Türstation");
    wifiSetupServer.setContentLength(html.length());
    wifiSetupServer.send(200,"text/html; charset=utf-8","");
    for(size_t offset=0;offset<html.length();offset+=512)wifiSetupServer.sendContent(html.substring(offset,offset+512));
  });
  wifiSetupServer.on("/update",HTTP_POST,[] {
    if(!otaAuthorized()){otaReject();return;}
    if(otaDenied){wifiSetupServer.send(403,"text/plain","Upload nicht autorisiert oder Station beschaeftigt");return;}
    wifiSetupServer.send(otaSucceeded?200:400,"text/plain; charset=utf-8",otaSucceeded?"Update erfolgreich. Neustart erfolgt.":"Update fehlgeschlagen. Bisherige Firmware bleibt aktiv.");
    if(otaSucceeded)otaRestartAt=millis()+1500;
  },[] {
    HTTPUpload& upload=wifiSetupServer.upload();
    if(upload.status==UPLOAD_FILE_START) {
      otaSucceeded=false;
      otaDenied=!otaAuthorized() || wifiSetupServer.arg("token")!=otaToken ||
          otaRestartAt || getIntercomState()!=IntercomState::IDLE || candidateActive;
      if(otaDenied)return;
      if(otaWriting)Update.abort();
      otaWriting=esp_ota_get_next_update_partition(nullptr)!=nullptr && Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH);
      otaLastData=millis();
    } else if(upload.status==UPLOAD_FILE_WRITE && otaWriting) {
      otaLastData=millis();
      if(Update.write(upload.buf,upload.currentSize)!=upload.currentSize){Update.abort();otaWriting=false;}
    } else if(upload.status==UPLOAD_FILE_END && otaWriting) {
      otaSucceeded=Update.end(true);otaWriting=false;
    } else if(upload.status==UPLOAD_FILE_ABORTED) {
      if(otaWriting)Update.abort();otaWriting=false;otaSucceeded=false;
    }
  });
}
static void handleOta() {
  if(otaWriting && (uint32_t)(millis()-otaLastData)>30000){Update.abort();otaWriting=false;}
  if(otaRestartAt && (int32_t)(millis()-otaRestartAt)>=0)ESP.restart();
}
