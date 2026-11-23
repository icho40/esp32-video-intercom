# WLAN über das Tablet einrichten

## Bedienung

Seit 06.11.2026 gibt es am Tablet keinen Menüpunkt „WLAN-Zugangsdaten ändern“ mehr. Das Zahnrad öffnet auf allen drei Tablets ohne PIN nur die Empfangsanzeige. Neue Zugangsdaten bekommt die Station über ihr Einrichtungs-WLAN „OpenIntercom-Setup“ (http://192.168.4.1), zum Beispiel mit einem Handy. Die folgenden Abschnitte beschreiben den früheren Tablet-Assistenten. Assistent, WLAN-Helfer (`wifihelper`) und die zugehörigen Tests wurden am 06.11.2026 aus der App entfernt; die Einrichtungsseite der Station selbst bleibt.

Wenn die Türstation über WLAN nicht erreichbar ist: Nach 60 Sekunden ohne WLAN öffnet sie das Einrichtungs-WLAN selbst. Hängt sie in einem anderen erreichbaren WLAN, bleiben Neustart mit Netzwahl oder USB. Die frühere Haltegeste (P1 fünf Sekunden) wurde am 06.11.2026 entfernt; alle Klingeltasten lösen beim Drücken aus.

Der Assistent übergibt dasselbe Ziel-WLAN an Tablet und Türstation. Die Station bestätigt zunächst nur den Empfang und wartet 2,5 Sekunden vor dem Wechsel. Das Tablet verbindet sich über den signaturgeschützten WLAN-Helfer. Anschließend sucht die App die Station mit ihrer Gerätekennung und prüft Ziel-SSID und Verbindungsstatus. Erst nach dieser Prüfung bestätigt die App die dauerhafte Speicherung der Türstationsdaten und beendet die vorläufige Tablet-Verbindung. Die neue Stationsadresse gilt für Steuerung, Bild und Audio.

## Rückkehr bei Fehlern

Ein fehlgeschlagener Tablet-Verbindungsversuch kehrt nach etwa 35 Sekunden zum bisherigen gespeicherten WLAN zurück. Die Station behält bis zur Bestätigung ihre bisherigen Zugangsdaten. Eine fehlgeschlagene Stationsverbindung wird nach 35 Sekunden verworfen; ohne Tablet-Bestätigung kehrt die Station spätestens nach 90 Sekunden zurück.

Solange die Tablet-Verbindung unbestätigt ist, beendet das Schließen der Einrichtung den Versuch. Zusätzlich sind eine unabhängige Rückkehr nach 110 Sekunden und eine Wiederherstellung beim Tablet-Neustart eingebaut. Ein Abbruch im kurzen Zeitraum zwischen dauerhafter Stationsbestätigung und abschließender Tablet-Bestätigung kann weiterhin eine manuelle Wiederherstellung erfordern; beide Geräte lassen sich dann über P1 bzw. ihre gespeicherten WLANs wieder verbinden.

Passwörter werden nicht protokolliert. Der Helfer legt WLAN-Zugangsdaten ausschließlich über Androids gespeicherte WLAN-Konfiguration an. Die App löscht die Passwortanzeige nach Erfolg. Bereits gespeicherte Ziel-WLAN-Profile werden wiederverwendet und deren Zugangsdaten nicht überschrieben. Bei veralteten Tablet-Zugangsdaten muss das betroffene WLAN-Profil deshalb zuvor korrigiert werden.

## Fire-Tablet und Installation

Fire OS 8 lässt den normalen WifiNetworkSpecifier-Dialog abstürzen. Die Gegensprechanlagen-App behält ihr aktuelles Android-Ziel; ein separat installierter Helfer mit Android-Ziel 28 verwendet die dokumentierte Kompatibilität der gespeicherten WLAN-Schnittstellen. Nur Apps mit derselben Signatur dürfen seinen Dienst aufrufen. Die sichtbare App übergibt ihre WLAN-/Standortfreigabe über BIND_INCLUDE_CAPABILITIES. Der Helfer benötigt einmalig die Standortberechtigung; diese wurde beim vorhandenen Tablet eingerichtet.

Für weitere Tablets müssen Haupt-App und wifihelper-APK mit derselben Signatur installiert und die erforderlichen Berechtigungen erteilt werden. Der Helfer hat keinen eigenen Startbildschirm; Fully bleibt bei Open Intercom.

Ist die Türstation im bestehenden Netz nicht erreichbar, verbindet „WLAN-Einrichtung starten“ das Fire-Tablet über den signierten WLAN-Helfer vorübergehend mit OpenIntercom-Setup. Dafür musste P1 an der Station fünf Sekunden gehalten werden (Geste am 06.11.2026 entfernt; heute öffnet die Station das Einrichtungs-WLAN nach 60 Sekunden ohne WLAN selbst). Android-WLAN-Einstellungen sind auch im Kioskmodus nicht nötig. Voraussetzung ist der aktualisierte Stand von Haupt-App und WLAN-Helfer. Abbruch, Verbindungsfehler oder Ablauf nach zehn Minuten stellen das ursprüngliche Tablet-WLAN wieder her. Beim anschließenden Wechsel ins Ziel-WLAN bleibt dieses ursprüngliche WLAN der Rückfall; erst die erfolgreiche Prüfung beider Geräte bestätigt den Wechsel. Am 24.10.2026 wurden Haupt-App und Helfer auf Tablet 1 per USB aktualisiert. Praktisch bestätigt: Wechsel aus Testnetz nach OpenIntercom-Setup innerhalb des Kioskmodus, Meldung „Türstation verbunden“ und geladene WLAN-Liste mit Testnetz, Heimnetz und weiteren Netzen. Ein anschließender Ziel-WLAN-Wechsel und die Rückkehr bei Abbruch wurden mit diesem neuen Einstieg noch nicht praktisch geprüft.

Offizielle Grundlage: https://developer.android.com/reference/android/net/wifi/WifiManager#addNetwork(android.net.wifi.WifiConfiguration)

## Geprüfter Stand am 11.10.2026

- Android-App, Helfer und Waveshare-Firmware bauen erfolgreich und sind installiert.
- Echter Fire-Gerätetest: gespeichertes WLAN auswählen und bestätigen; nicht vorhandenes Test-WLAN versuchen; automatische Rückkehr zum bisherigen WLAN. Erfolgreich.
- Echter Waveshare-Test: Wechsel von Testnetz nach [lokales WLAN] ohne Bestätigung; Status awaiting_confirmation; nach 90 Sekunden zurück in Testnetz. Erfolgreich.
- Vollständiger App-Gerätetest: Station von Testnetz nach [lokales WLAN] wechseln, Ziel-SSID und Gerätekennung prüfen, Bestätigung und Adressübernahme. Erfolgreich. Das Tablet war bei diesem Test bereits in [lokales WLAN].
- Waveshare nach finalem Firmware-Upload neu gestartet: gespeichertes [lokales WLAN] bleibt erhalten; Discovery und Steuerverbindung über 192.168.x.x erreichbar.
- Der Nutzer hat anschließend den gemeinsamen Wechsel zu Testnetz einschließlich Klingel, Bild und Audio erfolgreich geprüft. Abbruch-/Neustartwiederherstellung müssen noch separat im praktischen Einsatz geprüft werden.

## Einrichtungszugang der Türstation

OpenIntercom-Setup: 192.168.4.1, Passwort intercomsetup. Der Webserver ist während des zehnminütigen Einrichtungsfensters auch über die bestehende Stationsadresse erreichbar. Ein neuer Verbindungsversuch verlängert dieses Fenster. Physische Aktivierung durch P1 oder ein fehlgeschlagener WLAN-Start öffnet die Einrichtung.

Die Browserfunktion „Speichern und neu starten“ bleibt ein separater Wartungsweg und stellt das Tablet nicht um. Ohne gespeicherte Konfiguration nutzt die Firmware die lokale secrets.h; Zugangsdaten gehören nicht in Git.

## Einrichtungsbutton im Tablet

Die nicht exportierte Einrichtungsansicht wird über das PIN-geschützte Menü geöffnet. „WLAN-Einrichtung starten“ sucht die Station und sendet WIFI_SETUP über die Steuerverbindung nach REGISTER. Die Firmware akzeptiert den Befehl nur für angemeldete Clients im Zustand IDLE und führt den WLAN-/Webserver-Start im Hauptloop aus. Die App wartet auf die Einrichtungs-API, bevor sie die WLAN-Auswahl freigibt. Der bestehende Steuerkanal vertraut lokalen angemeldeten Clients; die Einstellungs-PIN wird lokal am Tablet geprüft. Das Einrichtungsfenster endet weiterhin nach zehn Minuten.

## Vereinfachte Ansicht

Die beiden manuellen Prüfungen sind nur unter „Hilfe bei Verbindungsproblemen“ erreichbar: „Verbindung zum Einrichtungs-WLAN prüfen“ und „Türstation im Heimnetz suchen“. Der normale Ablauf verwendet „WLAN-Einrichtung starten“ und „WLAN verbinden und prüfen“. Das Hilfemenü bleibt während eines laufenden Wechsels geschlossen.

Auch die manuelle WLAN-Suche wurde aus der Hauptansicht entfernt. Unter „Hilfe bei Verbindungsproblemen“ steht „WLAN-Liste aktualisieren“ bereit, sobald die Einrichtung verbunden ist. Die erste Suche startet weiterhin automatisch.

## Kiosk-Meldungen auf Tablet 1

Am 24.10.2026 wurde für `com.fullykiosk.singleapp` die Android-App-Operation `TOAST_WINDOW` von Standard (`allow`) auf `ignore` gesetzt. Die installierte Fully-APK enthält den Text „Network connection lost“; Fully-Toasts sollen beim erwarteten WLAN-Wechsel nicht zusätzlich zur App-Statusanzeige verwirren. Dies unterdrückt alle Fully-Toasts auf diesem Tablet, nicht nur Netzwerkmeldungen. Status- und Fehlermeldungen von Open Intercom bleiben unverändert. Der gesetzte Modus wurde per ADB zurückgelesen; die visuelle Wirkung beim nächsten WLAN-Wechsel ist noch zu prüfen. Rücknahme: `adb -s [Tablet-1-USB-ID] shell appops set com.fullykiosk.singleapp TOAST_WINDOW default`.

Die Toast-Sperre wurde am selben Tag nach einer vom Nutzer gemeldeten erneuten Fully-Berechtigungsabfrage auf `default` zurückgesetzt. Der genaue Zusammenhang ist nicht nachgewiesen; Overlay und Nutzungszugriff waren weiterhin erlaubt. Die pauschale App-Op-Sperre wird nicht als dauerhafte Lösung verwendet. Eine gezielte Fully-Einstellung für Netzwerk-Toasts ist noch zu prüfen.

## Zusätzliches Standort-WLAN vorbereiten (24.10.2026)

Der neue Button „Weiteres WLAN hinterlegen“ speichert ein zusätzliches WPA/WPA2-Profil auf der Türstation und auf dem gerade bedienten Tablet, ohne einen Verbindungswechsel anzufordern. Das neue Netz muss dafür nicht erreichbar sein. Ein vorhandenes zweites Stationsprofil wird ersetzt; das primäre Profil bleibt erhalten. Weitere Tablets müssen gesondert vorbereitet werden.

Die Türstation hält das zusätzliche Profil im NVS-Namensraum `intercom-wifi` unter `additional`; der bestehende Schlüssel `credentials` bleibt unverändert. Beim Start wird zuerst das primäre Profil versucht, nach 15 Sekunden ohne Verbindung das zusätzliche. Bei späterem Verbindungsverlust werden die Profile in 30-Sekunden-Abständen abwechselnd versucht. Nach 60 Sekunden durchgehendem Ausfall wird zusätzlich das Einrichtungs-WLAN geöffnet. Laufende, noch unbestätigte WLAN-Wechsel werden durch diese Profilwahl nicht unterbrochen. Offline bleibt der Einrichtungs-AP verfügbar, statt nach zehn Minuten abgeschaltet zu werden.

Die neue Einrichtungs-API meldet `additionalWifi=true`, `GET /api/profiles` liefert ausschließlich Profilnamen, und token-geschütztes `POST /api/profiles` speichert das zweite Profil ohne Neustart oder WLAN-Wechsel. Der Android-Helferbefehl 6 speichert das Profil über WifiManager und aktiviert es ohne `disableOthers` oder `reconnect`. Die App prüft zuerst die Firmware-Fähigkeit; Teilfehler werden ausdrücklich angezeigt.

Validiert: Firmware für das bestehende ESP32-S3-Board gebaut; App, WLAN-Helfer und vorhandene Instrumentierungstests kompiliert; nativer Test `tests/wifi_profile_recovery_test.cpp` ausgeführt (Zeitgrenzen, Wiederverbindung, laufender Kandidat, millis-Überlauf). Noch kein Flash, keine Installation und keine Speicherung des neuen Standortprofils. Reale automatische Verbindung am zukünftigen Standort ist noch nicht geprüft. Zugangsdaten sind nicht im Quellcode oder dieser Dokumentation enthalten.

## Abschließender Gerätecheck am 24.10.2026

Nach ausdrücklicher Freigabe wurde ausschließlich der Programmteil der Türstation aktualisiert und jeweils per Flash-Hash verifiziert. NVS und Partitionstabelle wurden nicht überschrieben. Das zusätzliche Standortprofil `Standortnetz` ist auf Tablet 1 und im NVS der Türstation gespeichert; sein Passwort wurde nicht in Projektdateien hinterlegt. Ein erneuter Stationsstart bestätigte das Laden des zusätzlichen Profils. Der erste erhaltene Hauptprofilname war noch `Test`; auf Nutzerkorrektur wurde das lokal vorhandene Profil `Heimnetz` als Hauptprofil ins NVS übernommen, während `Standortnetz` erhalten blieb.

Final nach Neustart bestätigt: `primary=Heimnetz additional=Standortnetz active=Heimnetz`, Stations-IP `192.168.x.x`. Tablet 1 ist ebenfalls in Heimnetz. Das Gerätetestverfahren zum Speichern ohne Verbindungswechsel bestand; Android bestätigte zusätzlich die Freigabe des gespeicherten Standortprofils für automatische Auswahl. Der Helfer aktiviert dieses Profil nach Bestätigung und Rückkehr von temporären Verbindungen erneut, da `enableNetwork(..., true)` sonst andere Profile deaktivieren kann. Der Android-Neustart-Fallback berücksichtigt dieses Zusatzprofil ebenfalls.

Der vorübergehende Tablet-Wechsel nach OpenIntercom-Setup konnte in diesem Durchlauf nicht zuverlässig hergestellt werden. Die tatsächliche Stationsspeicherung erfolgte deshalb über den neu ergänzten lokalen USB-Wartungsbefehl `WIFI_ADD <SSID>\t<Passwort>\n`. Dieser gibt ausschließlich Erfolg oder Fehler aus. `WIFI_PROFILES` zeigt nur Namen und aktive SSID; `WIFI_PRIMARY_DEFAULT` übernimmt das lokale Firmware-Standardprofil ins primäre NVS-Profil und verbindet damit. Diese Wartungsbefehle ersetzen keine allgemeine Werksrücksetzung.

Ein realer Verbindungsversuch mit Standortnetz und ein Standortwechsel können hier ohne das zukünftige Netz noch nicht bestätigt werden. Weitere Tablets wurden nicht vorbereitet. Die gewünschte gezielte Abschaltung von Fully-Netzwerk-Toasts ist weiterhin nicht bestätigt; die pauschale Toast-App-Op bleibt auf Standard.

## Tablet 2 vorbereitet (25.10.2026)

Tablet 2 (KFTUWI, USB-Kennung [Tablet-2-USB-ID]) wurde mit aktuellem App- und WLAN-Helferstand aktualisiert. Standortnetz wurde zusätzlich gespeichert und von Android zur automatischen Auswahl freigegeben. Der Instrumentierungstest `provisionAdditionalProfileWithoutSwitch` bestand; aktives Netzwerk und IP-Adressen blieben während der Speicherung unverändert. Abschließend bestätigt: weiterhin Heimnetz, gespeicherte WLAN-Namen Heimnetz und Standortnetz, `button_number=2`, Stationskennung `[Stations-MAC]` und Stationsadresse `192.168.x.x`. Die App enthält die automatische Stationssuche anhand der Gerätekennung. Der reale IP-Wechsel im zukünftigen Standortnetz ist weiterhin ungetestet. Die temporäre Datei mit Zugangsdaten wurde vom Test gelöscht und ihr Fehlen geprüft. Die Haupt-App ist wieder im Vordergrund. Tablet 3 ist noch vorzubereiten.

## Tablet 3 vorbereitet (25.10.2026)

Tablet 3 (KFTUWI, USB-Kennung [Tablet-3-USB-ID]) wurde mit demselben App- und WLAN-Helferstand wie Tablet 1 und 2 aktualisiert. Standortnetz ist zusätzlich gespeichert und von Android zur automatischen Auswahl freigegeben. Der Gerätetest `provisionAdditionalProfileWithoutSwitch` bestand (aktives Netzwerk und IP unverändert). Abschließend geprüft: Tablet weiterhin in Heimnetz, gespeicherte Profile Heimnetz und Standortnetz, unveränderte Zuordnung `button_number=3`, Stationskennung `[Stations-MAC]` und Adresse `192.168.x.x`. Die temporäre Zugangsdaten-Datei wurde gelöscht; Haupt-App ist wieder gestartet. Damit sind alle drei Tablets und die Türstation für das zusätzliche Standort-WLAN vorbereitet. Eine echte Verbindung aller Geräte mit Standortnetz wurde noch nicht getestet.

## Umstellung auf ausschließlich Standortnetz (25.10.2026)

Auf ausdrücklichen Nutzerwunsch wurde der fest eingebaute WLAN-Rückfallzugang aus der aktiven Türstationsfirmware entfernt (kein Einbinden von secrets.h mehr; ohne NVS-Profil kein Standard-Heimnetz). Der alte Wartungsbefehl WIFI_PRIMARY_DEFAULT entfällt. Der neue USB-Befehl `WIFI_ONLY <gespeicherte SSID>` übernimmt ein vorhandenes Profil als Hauptprofil, entfernt das Zusatzprofil und löscht die bisherige WLAN-Treiberkonfiguration vor dem Verbinden. Er gibt keine Passwörter aus.

Firmware erfolgreich für die bestehende Boardkonfiguration gebaut, Programmteil geflasht und Hash verifiziert. `WIFI_ONLY Standortnetz` mit WIFI_ONLY_STORED bestätigt. Danach primary=Standortnetz, additional leer, active=Standortnetz, IP 192.168.x.x. Bei einem folgenden Start schlug der erste Verbindungsversuch fehl; beim weiteren Start verband sich die Station automatisch mit Standortnetz. Tablet 1 und Tablet 2 meldeten sich im Kontrollserver als P1/P2 aus 192.168.x.x/192.168.x.x an. Die neue Firmwaredatei enthält den Namen Heimnetz nicht mehr.

Tablet 1 ([Tablet-1-USB-ID]): per Android cmd wifi die gespeicherten Profile Heimnetz, Test, Testnetz und OpenIntercom-Setup entfernt. Ausschließlich Standortnetz gespeichert und verbunden. Tablet 2 ([Tablet-2-USB-ID]): Heimnetz entfernt, ausschließlich Standortnetz gespeichert und verbunden. Tablet 3 steht zu diesem Zeitpunkt noch aus.

Abschließend auch Tablet 3 ([Tablet-3-USB-ID]): Heimnetz entfernt, list-networks enthält ausschließlich Standortnetz, status bestätigt Standortnetz verbunden. Stationslog bestätigt P3 aus 192.168.x.x. Damit alle drei Tablets als P1/P2/P3 an der Türstation im Standortnetz angemeldet; Stationsprofil nach Neustart primary=Standortnetz additional= active=Standortnetz. Die Kamerastörung ist davon unabhängig noch offen (UART-Bytes vorhanden, Audiopakete werden verworfen); kein erfolgreicher Audio-/Videofunktionstest behauptet.

## WLAN-Diagnose (25.10.2026)

Die Firmware stellt dauerhaft GET /diagnose und GET /api/diagnose auf Port 80 bereit. Nur lesend, keine Zugangsdaten: WLAN/IP/RSSI/Kanal/BSSID, Laufzeit, Zustand, Tablet-Steuerverbindungen mit Zuordnung, UART-Bytes, Kamera-/Audiozähler, Alter und Größe des letzten Kamerapakets, verworfene Pakete und freier Heap. HTML aktualisiert alle zwei Sekunden ohne überlappende Abfragen; bei Fehlern werden Werte als möglicherweise veraltet gekennzeichnet. Die Verworfen-Zahl umfasst ungültige UART-Pakete und Audio-Queue-Verluste, nicht WLAN-Paketverluste. Kamerapakete sind keine JPEG-Qualitätsprüfung.

Der bestehende HTTP-Server bleibt nach Ende des Einrichtungsmodus aktiv. Alle bisherigen Einrichtungsrouten sind außerhalb des aktivierten Einrichtungsmodus gesperrt (403). Diagnosezähler sind atomar, Tablet- und Bildstatus werden unter den vorhandenen Sperren kopiert. Firmware gebaut und Flash-Hash geprüft; JSON-Livedaten und HTTP-403-Sperre am Gerät bestätigt.

Abschlussprüfung: HTML vollständig mit HTTP 200 (3212 Bytes), JSON HTTP 200; Seite im Browser gerendert und automatische Aktualisierung beobachtet. HTML wird in 512-Byte-Blöcken ausgeliefert. Aktueller Zugriff: http://192.168.x.x/diagnose. Bestehende Empfangsprobleme sind weiterhin sichtbar (wenige Kamerapakete, steigendes Bildalter und Verworfen-Zähler); die Diagnosefunktion behebt diese nicht.

## OTA und Neustartgrund (25.10.2026)

Nach ausdrücklicher Administratorfreigabe: /update ermöglicht Firmware-Uploads mit HTTP-Digest-Anmeldung (Benutzer admin, individuelles zufälliges Gerätepasswort im NVS-Namensraum intercom-ota). Zugriff nur bei aktiver SSID Standortnetz über die Stations-IP und aus deren IPv4-Subnetz; keine Internetfreigabe eingerichtet. Das lokale Subnetz ist eine Eingrenzung, keine zusätzliche Benutzeridentität; das Passwort bleibt erforderlich. Der USB-Befehl OTA_ACCESS gibt die Zugangsdaten nur lokal auf COM aus. Mit OTA_SET_PASSWORD <passwort> (8–64 druckbare ASCII-Zeichen ohne Leerzeichen) wird über USB ein eigenes Passwort gesetzt; Antwort OTA_PASSWORD_STORED bzw. OTA_PASSWORD_ERROR, ohne Echo. Die lokale Administrator-Kopie liegt außerhalb des Repositorys unter ~/.local/share/open-intercom/doorstation-ota.json (0600).

Upload erfordert zusätzlich einen zufälligen Sitzungstoken und IDLE-Zustand. Verwendet wird Update/U_FLASH mit dem nächsten OTA-Slot; fehlgeschlagene/abgebrochene Uploads starten nicht neu. Eine passende .ino.bin verwenden, kein merged.bin. Die tatsächlich ausgelesene Partitionstabelle bestätigt app0 0x10000 und app1 0x340000 mit jeweils 0x330000 Bytes. Es gibt keine zugesicherte automatische Rückkehr bei einer zwar gültigen, aber fehlerhaften neuen Firmware. USB bleibt der Wiederherstellungsweg.

Diagnose liefert reset_reason/reset_code über esp_reset_reason() sowie firmware_slot. Das beschreibt jeweils den letzten Start, nicht die historischen Abstürze vor der Installation. Build erfolgreich, USB-Flash per Hash verifiziert. Gerätetest: ohne Anmeldung 401, falscher Sitzungstoken 403, ungültiges Image 400; laufende Firmware danach weiterhin im app0 erreichbar.

End-to-end bestätigt: WLAN-Upload HTTP 200, anschließend automatischer Software-Neustart (reset_code=3), firmware_slot=app1, Standortnetz und unverändertes Administratorpasswort. P1 und P3 wieder angemeldet. Der erste ungedrosselte Upload wurde unterbrochen; die Station blieb auf app0 ohne Neustart. Gedrosselter Upload (1024 Bytes je 20 ms) funktionierte. Dafür liegt tools/ota_update.py bereit: `python3 tools/ota_update.py /pfad/doorstation_v2.ino.bin`. Benötigt requests; liest Zugang lokal aus der geschützten Administrator-Datei. Browser-Upload ist eingebaut, bei instabiler Verbindung das gedrosselte Skript verwenden. Geprüfte Firmwarekopie: ~/.local/share/open-intercom/doorstation-ota.bin. Nach OTA startet jeweils der andere Slot; künftige USB-Recovery muss aktive OTA-Bootauswahl berücksichtigen, nicht blind nur app0 überschreiben.


## WLAN-Auswahl beim Start (30.10.2026)

Nach dem Einschalten beginnt vor der WLAN-Verbindung ein 20-Sekunden-Fenster. Ein mindestens 40 ms stabiler einzelner Druck auf P1 wählt Test-WLAN, auf P2 Haus-WLAN. Der erste gültige Druck beendet das Fenster sofort. Gleichzeitige P1/P2-Betätigung wird ignoriert, bis genau eine Taste stabil gedrückt ist. Ohne Auswahl verwendet die Station nach 20 Sekunden das zuletzt erfolgreich verbundene gespeicherte WLAN. Beim ersten Start ohne Historie wird das vorhandene Hauptprofil verwendet.

Die Wahl verweist auf bereits gespeicherte Profile; WLAN-Kennwörter stehen nicht im Code. Fehlt ein gewähltes Profil, meldet die USB-Ausgabe dies und verwendet ein gespeichertes Profil. Die Bootauswahl verschiebt oder löscht keine Zugangsdaten. Scheitert der erste Verbindungsversuch nach 15 Sekunden, wird das andere gespeicherte Profil versucht; nach weiterem Fehlschlag öffnet das Einrichtungs-WLAN. Bestehende Wiederverbindungsregeln bleiben aktiv.

Das zuletzt erfolgreich verbundene Profil wird unter intercom-wifi/lastssid gespeichert, nur bei Änderung und nicht während eines unbestätigten WLAN-Kandidaten. Alle Tasten werden nach der Auswahl erst wieder als Klingeltaster ausgewertet, wenn sie losgelassen sind. P1 fünf Sekunden halten war damals im normalen Betrieb die Einrichtungsgeste (am 06.11.2026 entfernt). Die Tablet-WLAN-Auswahl wird dadurch nicht ferngesteuert.

Die Firmwarekennung lautet 812524a-bootwifi-20261030. Auswahl-Logiktests bestanden: P1, P2, 20-Sekunden-Grenze, Tastenprellen, gleichzeitige Tasten, gespeicherte Entscheidung und millis-Überlauf. Firmware gebaut, app0 geflasht und Hash geprüft. Reales Startfenster ohne Tastendruck: Haus-WLAN verbunden, beide Profilnamen erhalten. Die physischen P1/P2-Auswahltests und eine tatsächliche Verbindung am Test-WLAN-Standort stehen noch aus.

OTA-Passwort auf direkten Nutzerwunsch lokal am Gerät geändert; kein Passwort in dieser Dokumentation. Kennwortänderung über USB akzeptiert jetzt 8–64 druckbare ASCII-Zeichen ohne Leerzeichen. Die bisherige OTA-Netzeingrenzung auf Haus-WLAN bleibt bestehen.

### Praktische Netzwahl bestätigt (30.10.2026)

Nach dem USB-Abziehen und Neustart ohne Tastendruck: Haus-WLAN verbunden. Danach Neustart mit physischer P1-Auswahl: Diagnose bestätigt Test-WLAN unter 192.168.10.107. Anschließend Neustart mit physischer P2-Auswahl: Diagnose bestätigt Haus-WLAN unter 192.168.179.3 und P1-Tablet angemeldet. Damit Timeout-Start sowie beide physischen Auswahlwege bestätigt. Die Messung des letzten Bildalters war bei den Auswahltests teilweise erhöht; daraus wird kein erfolgreicher vollständiger Audio-/Videotest abgeleitet.


## Schlanker Wartungszugang am EG-Tablet (30.10.2026)

Zahnrad entfernt. WG-Logo fünf Sekunden halten, vorhandene Einstellungs-PIN eingeben. Danach nur zwei Menüpunkte: WLAN-Zugangsdaten ändern und WLAN-Empfang anzeigen. P2/P3 erhalten keinen Wartungszugang. Die Klingelzuordnung ist über dieses Menü nicht mehr veränderbar.

Seit 05.11.2026: Die Haltegeste auf dem WG-Logo war in der Praxis unzuverlässig (Abbruch schon bei kleiner Fingerbewegung, keine Rückmeldung während der fünf Sekunden). Sie ist ersetzt durch ein sichtbares Zahnrad rechts oben, nur auf P1 und nicht während Klingeln/Sprechen; Antippen öffnet die Einstellungs-PIN. Auf Tablet 1 per USB installiert und geprüft: Zahnrad sichtbar, PIN-Abfrage öffnet sich.

WLAN-Zugangsdaten ändern öffnet die bestehende gemeinsame WLAN-Einrichtung mit automatischer Stationssuche. Die sichtbaren Zusatzprofil- und Hilfemenüs entfallen. RSSI wird separat angezeigt; Schließen kehrt direkt zur Gegensprechanlage zurück. Bereits gespeicherte Profile und die PIN bleiben erhalten. Historische Abschnitte weiter unten beschreiben teilweise die frühere Oberfläche.

### Stations-IP und Empfangsampel (30.10.2026)

WLAN-Empfang anzeigen enthält die aktuelle Stations-IP, die Diagnoseadresse und den Button Diagnoseseite öffnen. Die Diagnoseseite wird innerhalb der App angezeigt, ohne Wechsel in eine vom Kiosk möglicherweise gesperrte Browser-App. Empfangsampel am Waveshare: Grün ab -65 dBm (gut bis sehr gut), Gelb -66 bis -75 dBm (gerade noch ausreichend), Rot unter -75 dBm (zu schwach). Zusätzlich dBm und Textbewertung. Ohne aktuelle Antwort grau, IP als letzte bekannte Adresse markiert, Diagnosebutton deaktiviert. Aktualisierung alle zwei Sekunden.
