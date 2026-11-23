# Gegensprechanlage – verbindliche Anforderungen

Stand: 6. November 2026 (Erstfassung 10. Oktober 2026)
Projekt: `esp32-open-intercom`, Git-Branch `main`

## Geräte und Zuordnung

Es gibt drei Android-Tablets, jeweils fest einer Türklingel zugeordnet:

| Tablet | Etage | Reagiert ausschließlich auf | Klingelton (App) |
| --- | --- | --- | --- |
| P1 | Erdgeschoss | `STATE RINGING_P1` | `res/raw/klingel_p1.mp3` |
| P2 | 1. Stock | `STATE RINGING_P2` | `res/raw/klingel_p2.mp3` |
| P3 | Keller | `STATE RINGING_P3` | `res/raw/klingel_p3.mp3` |

Andere Klingelsignale dürfen auf einem Tablet weder Klingelton noch Benachrichtigung noch Livebild auslösen.

**Umgesetzt und geprüft (06.11.2026):** Jedes Tablet reagiert nur auf seine Klingel. Die anderen Tablets erhalten die Zustandsmeldung, reagieren aber nicht darauf.

## Gewünschter Ablauf

1. Das Tablet zeigt die Bereitschaftsansicht. Livebild und Audio sind nicht aktiv.
2. Die zugeordnete Klingel wird gedrückt. Alle Klingeltasten (P1, P2, P3) lösen **beim Drücken** aus.
3. Nur auf diesem Tablet ertönt der zugehörige Klingelton. Livebild und Audio der Türstation starten.
4. **„Sprechen“ gedrückt halten** nimmt den Ruf an und sendet das Tablet-Mikrofon zur Türstation. Währenddessen ist die Wiedergabe am Tablet stumm. Loslassen schaltet zurück auf Zuhören.
5. Die Taste **Ende** beendet Livebild und Audio. Ohne Annahme endet der Ruf nach 30 Sekunden.
6. Das Tablet zeigt wieder die Bereitschaftsansicht.

Die Steuerverbindung zum Waveshare bleibt im Hintergrund bestehen, damit Klingelereignisse empfangen werden.

**Umgesetzt und geprüft (06.11.2026):** Klingeltest auf allen drei Tablets bestanden. Es klingelt, Bild und Ton kommen, Gegensprechen geht.

### Festgelegtes Verhalten

- **Nur ein Ruf gleichzeitig.** Solange eine Klingel klingelt oder ein Gespräch läuft, werden andere Klingeltasten ignoriert und nicht vorgemerkt.
- **Klingelton** spielt einmal und endet von selbst. Die Dauer ist je nach Etage 1 bis 3,5 Sekunden. Eine Bestätigung ist nicht nötig. Damit ist die frühere offene Frage erledigt.
- **Kein Bedienelement von Android** auf dem Tablet. Die Navigationsleiste (Zurück, Home, Übersicht) bleibt auch nach Klingeln und Gespräch ausgeblendet.
- **Keine Haltegeste an der Türstation.** Die frühere Geste „P1 fünf Sekunden halten öffnet die WLAN-Einrichtung“ ist seit 06.11.2026 entfernt.

## WLAN

- Türstation und Tablets müssen im selben WLAN sein.
- Die Türstation verbindet sich mit dem **stärksten** Zugangspunkt bzw. Repeater des WLANs. Bei schwachem Signal (unter −70 dBm) wechselt sie im Ruhezustand zu einem mindestens 10 dB stärkeren Repeater.
- Die Tablets finden die Station über ihre Gerätekennung (`XX:XX:XX:XX:XX:XX`). Bei einem Board-Wechsel muss die Kennung auf allen Tablets neu eingetragen werden, mit `tools/tablet_station_fix.sh`.
- Das Zahnrad auf jedem Tablet öffnet ohne PIN nur die Empfangsanzeige der Türstation. Ein Ändern des WLANs über das Tablet ist nicht vorgesehen.
- Neue WLAN-Zugangsdaten: Nach 60 Sekunden ohne WLAN öffnet die Station das Einrichtungs-WLAN „OpenIntercom-Setup“ selbst. Mit einem Handy verbinden und http://192.168.4.1 öffnen. Sonst per USB.
- Beim Einschalten wählt ein Druck in den ersten 20 Sekunden das Netz: P1 = Test-WLAN, P2 = Haus-WLAN. Ohne Druck nimmt die Station das zuletzt erfolgreiche WLAN.

## Umsetzung und Bestandsschutz

- Es gibt eine gemeinsame Android-App/APK. Die Klingelnummer (P1/P2/P3) wird pro Tablet eingestellt. Umgesetzt.
- Vorhandene, funktionierende Video- und Audioübertragung nicht unnötig verändern.
- Aktueller geprüfter Stand: Branch `main`, Commit `4ff6cb4` (06.11.2026). Frühere Prüfstände sind als Tags erhalten:
  - `bild-und-ton-funktioniert-2026-10-10`
  - `bild-und-ton-komplett-2026-10-10`
  - `android-p1-funktioniert-2026-10-11`
- Änderungen schrittweise und prüfbar vornehmen. Nach jeder Änderung einen Klingeltest auf dem betroffenen Tablet durchführen.

## Noch offen

- **Kamera:** Der XIAO liefert zeitweise keine Bilder, nur Ton. Die Ursache ist ungeklärt.
- **Repeater-Wechsel** am Einbauort mit schwachem Signal prüfen.
- **Feste IPs** im Haus-WLAN vom Admin abwarten.
- **`docs/Gebrauchsanleitung.pdf`** wird mit `python3 tools/make_manual_pdf.py` aus `Gebrauchsanleitung.md` erzeugt.

Details zum jeweiligen Prüfstand: `docs/stand-2026-11-06.md`.
