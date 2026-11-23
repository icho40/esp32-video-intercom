# Architektur der aktuellen V2

Stand: 28.10.2026. Siehe auch die [Projektübersicht](../README.md).

## Türstation

XIAO ESP32-S3 Sense und Waveshare befinden sich zusammen mit der Steuerplatine im selben Gehäuse. Der 5-V/GND-Eingang der Steuerplatine versorgt beide Boards; die UART-Signale arbeiten mit 3,3-V-Pegeln.

Die XIAO erfasst JPEG-Kamerabilder und PCM16-Mikrofonton (16 kHz, mono). Ein separater Aufnahmetask puffert Audio; der UART-Sender bevorzugt Audiopakete vor Kamerabildern. Der aktuelle UART-Link überträgt mit 1 Mbit/s von der XIAO zum Waveshare. Die XIAO verwaltet keine Klingeltaster oder Rufzuordnungen.

Der Waveshare liest P1/P2/P3 an GPIO4/5/6, empfängt UART an GPIO18 und verwaltet IDLE, RINGING_P1/P2/P3 und TALKING mit zugeordneter Partei. Er puffert das letzte Kamerabild und verteilt Mikrofonton an die passenden registrierten Innenstationen.

Tablet-Sprache geht direkt vom Waveshare über I²S zum MAX98357A und Lautsprecher. Sie wird nicht über die XIAO zurückgeführt.

## WLAN und Datenwege

| Verbindung | Transport | Inhalt |
|---|---|---|
| XIAO → Waveshare | UART, 1 Mbit/s | JPEG- und PCM-Audiopakete |
| Waveshare → Android-App | TCP 5000 | JPEG-Bilder mit 4-Byte-Längenpräfix, little endian; bis zu 4 Geräte gleichzeitig (z. B. EG-Tablet und Handy an P1) |
| Android-App ↔ Waveshare | TCP 5001 | Registrierung, Rufzustand, ANSWER, HANGUP, PING sowie Wartungsabfragen |
| Waveshare → zugeordnete App | UDP 5002 | PCM16-Audio, 16 kHz, mono |
| App → Waveshare | UDP 5003 | Sprache bei gedrückter Sprechtaste |
| App ↔ Waveshare | UDP 5004 | Stationssuche anhand Gerätekennung |
| Browser/App ↔ Waveshare | HTTP 80 | Diagnose, zeitweise WLAN-Einrichtung, geschütztes OTA |

Ein lokaler WLAN-Router/Access Point verbindet die Geräte. Die Steuerverbindung registriert eine Innenstation als P1, P2 oder P3. Die Zuordnung legt fest, welches Tablet klingelt und Audio erhält. Die Kamera ist für alle Parteien dieselbe.

## Android-Innenstationen

Die Tablet-App verwaltet Livebild, Wiedergabe und Aufnahme entsprechend dem Rufzustand. Während die Sprechtaste gehalten wird, sendet das Mikrofon zur Türstation und die lokale Wiedergabe wird stummgeschaltet. Loslassen stoppt das Mikrofon, Ende sendet HANGUP. Unbeantwortete Rufe enden nach 30 Sekunden. Die Firmware gibt ein angenommenes Gespräch frei, wenn alle passenden registrierten Steuerverbindungen verloren gehen.

Zuordnung: P1 Erdgeschoss, P2 1. Stock, P3 Keller. Fully Single App Kiosk übernimmt den Tablet-Kioskbetrieb. Alle Tablets zeigen das Zahnrad; es öffnet ohne PIN die WLAN-Empfangsanzeige der Türstation.

Die separate Handy-App ist eine P1-Testversion im lokalen WLAN und hat keinen Kioskmodus. Die praktische Bestätigung der korrigierten Sprechtastenfunktion steht aus.

## WLAN-Einrichtung, Diagnose und OTA

Die Tablets ändern das WLAN der Station nicht mehr; Assistent und WLAN-Helfer wurden am 06.11.2026 entfernt. Neue Zugangsdaten bekommt die Station über ihr Einrichtungs-WLAN „OpenIntercom-Setup“ (http://192.168.4.1), das sie nach 60 Sekunden ohne WLAN öffnet. Die aktuelle Stationsfirmware lädt WLAN-Profile aus NVS, ohne private Zugangsdaten im Quellcode.

WIFI_RSSI fragt nur die Empfangsstärke ab. Der Tablet-Dialog wiederholt die Abfrage alle zwei Sekunden, ohne ein Audioziel zu registrieren oder das Einrichtungs-WLAN zu öffnen.

Die ständig verfügbare Diagnose zeigt unter anderem WLAN, Laufzeit, Rufzustand, Tablet-Verbindungen sowie UART-/Kamera-/Audiozähler. OTA ist ein angemeldeter, token-geschützter Wartungsweg für den Waveshare; die XIAO bleibt ein separates USB-Updateziel.

Details und Prüfgrenzen: [WLAN-Einrichtung](wlan-einrichtung.md). Der dort zuletzt dokumentierte UART-Kamera-/Audiofehler nach dem Standortwechsel ist weiterhin als offen zu behandeln.

## Historische Dateien

Die Browser-/MJPEG-Variante unter firmware/waveshare_panel und frühere Architekturideen sind nicht der aktuelle Tablet-Datenweg. MQTT, Home Assistant, KI-Ereignisse und Türöffnersteuerung sind nicht als implementierte Bestandteile dieses V2-Aufbaus bestätigt.
