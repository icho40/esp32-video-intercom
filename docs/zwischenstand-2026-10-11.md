# Zwischenstand 11.10.2026

## Verhalten

- Tablet-Zuordnung P1, P2 oder P3 ist gespeichert und im Bereitschaftsbildschirm sichtbar. Auswahl durch langen Druck auf den Bereitschaftsbereich.
- Bis zu acht parallele Steuerverbindungen mit REGISTER P1/P2/P3 und PING/PONG. Ein Ruf zur Zeit; weitere Klingeltastendrücke während Ruf oder Gespräch werden ignoriert.
- Unbeantwortete Rufe enden nach 30 Sekunden. Erstes Drücken der Sprechtaste sendet ANSWER; TALKING_P1/P2/P3 bleibt bis HANGUP aktiv.
- Nur Tablets mit passender Zuordnung erhalten Türstationsaudio und dürfen den Ruf annehmen/beenden. Der Rückkanal akzeptiert nur Audiopakete von einer passenden angemeldeten IP-Adresse; dies ist keine kryptografische Authentifizierung.
- Sprechen rechts neben Video: Mikrofon nur beim Gedrückthalten. Ende links neben Video.
- Temporärer Tischtest: Tablet-Wiedergabe während des Sprechens stumm. Im Produktivbetrieb soll sie dauerhaft an sein; Umstellung und Test am getrennten Einbauort stehen aus.
- Steuerantwort STATE UNCHANGED wird nicht als Rufzustand übernommen.
- Ein angenommenes Gespräch wird freigegeben, wenn keine verbundene Tablet-Zuordnung zum Ruf mehr existiert. Erkannter TCP-Abbruch führt zu IDLE; stille Ausfälle werden durch die vorhandene 45-Sekunden-Verbindungsüberwachung erkannt.

## Audio und Gerät

XIAO: Mikrofonaufnahme getrennt vom UART-Sender, Audio-Vorrang, 64 Audio-Pufferplätze, höchstens 5 Videobilder/s. UART bleibt 1 Mbit/s. Nutzer bestätigt flüssigen Ton bei leicht ruckelndem Bild; XIAO im Betrieb ohne USB-Verbindung testen.

PCM: 16 kHz, 16 Bit, mono. Türstation → Tablet UDP 5002; Tablet → Türstation UDP 5003, 512 Byte/Paket. JPEG über TCP 5000, Steuerung TCP 5001.

Fire-Tablet: Fully Single App Kiosk; Autostart nach Neustart und Rückkehr nach Home vom Nutzer bestätigt. Die Intercom-App ist nicht mehr als HOME-Launcher registriert. Die frühere native Kioskverwaltung bleibt im Quellcode, ist auf dem Fire-Hauptbenutzer nicht aktiv.

## Prüfung

- Android assembleDebug erfolgreich, APK auf Testtablet installiert.
- Waveshare-Firmware kompiliert, aufgespielt und Flash-Hash geprüft. Boardoptionen: esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi,CDCOnBoot=default.
- XIAO-Firmware kompiliert und aufgespielt; esp32:esp32:XIAO_ESP32S3:PSRAM=opi.
- Drei parallele Testclients melden sich als P1/P2/P3 an und beantworten PING mit PONG.
- Echtes Testtablet meldet REGISTERED P1. Fremder P2-Ruf aktiviert das P1-Tablet nicht.
- 30-Sekunden-Rufende vom Nutzer bestätigt. Protokoll zeigt erfolgreiche Annahme als TALKING_P1.
- STATE-UNCHANGED-Fehler behoben und App aktualisiert. Letzte seitliche Ende-Anordnung vom Nutzer bestätigt.

## Noch offen

Vollständiger Ablauf mit drei gleichzeitig verbundenen echten Tablets, wiederholtes Sprechen nach der Zustandskorrektur sowie Produktivtest ohne Wiedergabestummschaltung. Audio an mehrere Tablets mit derselben Zuordnung wird unterstützt; der Videoserver bedient weiterhin einen Client zur Zeit. Angenommene Rufe haben noch kein maximales Gesprächslimit. Beim Verlust aller passenden Tablet-Verbindungen werden sie automatisch beendet.

Für weitere Tablets die aktuelle App installieren und ihre Zuordnung auswählen. Ältere Apps ohne REGISTER/PING sind mit dieser Türstationsversion nicht kompatibel. Fully-Einstellungen und lokale WLAN-Zugangsdaten sind separat zu sichern.

## Ergänzung: Gesprächsfreigabe bei Verbindungsabbruch

Firmware erfolgreich kompiliert, auf Waveshare installiert und Flash-Hash geprüft. Live-Test mit physischem P3-Tastendruck und mehreren TCP-Testclients bestanden:

- P2 darf einen P3-Ruf nicht annehmen.
- P3 wird angenommen und als TALKING_P3 gemeldet.
- Trennen einer fremden P2-Verbindung beendet P3 nicht.
- Solange ein zweites P3-Tablet verbunden ist, bleibt das Gespräch aktiv.
- Trennen des letzten P3-Clients führt automatisch zu IDLE.

Der 45-Sekunden-Watchdog besteht unverändert; ein tatsächlicher stiller WLAN-Ausfall wurde bei dieser Ergänzung nicht separat getestet. Die Android-App benötigt für diese Ergänzung kein Update.
