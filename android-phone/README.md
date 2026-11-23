# Open Intercom – Handy-Apps

Handy-Apps im lokalen WLAN, je eine eigene App pro Klingel (aus demselben Code, Gradle-Varianten p1/p2/p3):

| App | Klingel | Paket | APK |
| --- | --- | --- | --- |
| Open Intercom EG | P1 Erdgeschoß | de.willi.openintercom.phone | app/build/outputs/apk/p1/debug/app-p1-debug.apk |
| Open Intercom 1. Stock | P2 | de.willi.openintercom.phone.p2 | app/build/outputs/apk/p2/debug/app-p2-debug.apk |
| Open Intercom Keller | P3 | de.willi.openintercom.phone.p3 | app/build/outputs/apk/p3/debug/app-p3-debug.apk |

Die Klingel ist fest in die jeweilige App eingebaut, es gibt keine Auswahl. Die EG-App behält das bisherige Paket, eine vorhandene Installation wird also aktualisiert. Geräte: Samsung S24 FE (Android 17) und Samsung A13 (Android 14). Livebild, Audio, Sprechtaste, Ende, Klingelbenachrichtigung. Kein Kioskmodus. Mehrere Geräte an derselben Klingel bekommen gleichzeitig Bild (Türstations-Firmware ab 06.11.2026).

Mikrofon und Benachrichtigungen direkt erlauben. Akku-Einstellung Uneingeschränkt. Nach Neustart App einmal öffnen. Zahnrad: Akku, Benachrichtigungen, Empfang beenden.

Installationsanleitung für Nutzer: ../docs/Handy-App-Installation.pdf (erzeugt aus der .md mit python3 tools/make_handy_pdf.py). Die dort genannten Dateien (OpenIntercom-EG.apk, OpenIntercom-1Stock.apk, OpenIntercom-Keller.apk) sind umbenannte Kopien der Build-Ausgaben und liegen lokal unter apk/ (nicht im Git). Für WhatsApp gibt es dort dieselben Dateien mit Endung .ppp, weil WhatsApp keine .apk verschickt; die Empfänger benennen sie laut Anleitung zurück.

Build aller drei Apps: ./gradlew :app:assembleP1Debug :app:assembleP2Debug :app:assembleP3Debug (Android SDK nötig, z. B. ANDROID_HOME setzen). Zum Aktualisieren mit demselben Schlüssel signieren. Auf Samsung-Handys mit „Automatische Sperre“ muss diese für die Installation per USB kurz ausgeschaltet werden.

Version 1.1: zusätzliche Tastenzustands- und Sitzungsprüfung vor Aufnahme und Audioversand. Der gemeldete Sprechtastenfehler ist bis zum praktischen Handytest weiterhin offen.

Siehe ../docs/stand-2026-10-17.md.
