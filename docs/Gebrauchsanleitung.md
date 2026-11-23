# Open Intercom

<!-- Fassungen: Bewohner (ohne Admin-Teile) und Admin. Zeilen mit "nur admin"/"nur bewohner" und Bloecke zwischen admin-Markierungen erzeugt tools/make_manual_pdf.py getrennt. -->

Gebrauchsanleitung | Drei Parteien | Stand 10. November 2026

Für die installierte Türstation und die Android-Tablets. Die Anlage arbeitet im lokalen WLAN. Für ein Gespräch müssen Türstation und Tablet einander im selben Netz erreichen können.

| Klingeltaste | Zugeordnetes Tablet |
| --- | --- |
| P1 | Erdgeschoss |
| P2 | 1. Stock |
| P3 | Keller |

## Im Alltag: klingeln, hören, sprechen

**1.** An der Türstation die gewünschte Klingeltaste **kurz drücken und loslassen**.

**2.** Das zugeordnete Tablet klingelt und zeigt das Kamerabild. Der Ton von der Türstation ist am Tablet hörbar.

**3.** Zum Antworten **„Sprechen“ gedrückt halten**. Der erste Druck nimmt den Ruf an. Das Tablet-Mikrofon sendet nur während des Haltens.

**4.** Zum Zuhören **loslassen**. Während du sprichst, ist der Ton der Türstation am Tablet stumm; nach dem Loslassen wird er wieder hörbar.

**5.** Mit **„Ende“** das Gespräch beenden. Die Bereitschaftsansicht mit Etagenbezeichnung erscheint wieder.

> **Keine Annahme:** Ein unbeantworteter Ruf endet nach 30 Sekunden und erscheint als **verpasster Besuch**. Ist eine Ansage eingeschaltet, spielt die Türstation sie vorher ab (siehe „Ansage bei Abwesenheit“). Während eines laufenden Rufs wird kein weiterer unabhängiger Ruf begonnen.

## Bereitschaft

Die App bleibt auf den Tablets im Kioskmodus geöffnet. Autostart und Home-Sperre werden durch Fully Single App Kiosk übernommen. Die Etagenzuordnung ist im normalen Menü nicht veränderbar.

Für Bild, Ton und Klingeln ist eine funktionierende WLAN-Verbindung nötig. Ein starkes Empfangssignal allein garantiert noch keine störungsfreie Verbindung.

> **Tablet immer am Strom lassen.** Mit Netzteil bleibt das Tablet verbindungsbereit und der Akku kann nicht leerlaufen. Ohne Strom schickt Android das Tablet bei dunklem Bildschirm nach einiger Zeit in einen Tiefschlaf; die App ist davon zwar ausgenommen, das Netzteil ist aber die sichere Lösung. Ein ausreichend starkes Netzteil verwenden, am besten das Original. Wölbt sich nach Jahren das Gehäuse oder hebt sich das Display, ist der Akku aufgebläht: Tablet tauschen.


---

# Ansage bei Abwesenheit

Ist niemand da, kann die Türstation dem Besucher einen kurzen Satz ansagen. Jede Partei schaltet das für ihre Klingel selbst ein und aus.

## Einschalten

**1.** Im Bereitschaftsbildschirm auf **„Ansage bei Abwesenheit“** tippen. In der zweiten Zeile des Buttons steht, was gerade gilt: **„aus“** (grau) oder der gewählte Satz (grün).

**2.** Unter **Satz** einen Satz auswählen. Mit **„Anhören“** lässt er sich vorher am Tablet anhören.

**3.** Unter **Klingeln** bei Bedarf **„Nicht klingeln, sofort ansagen“** ankreuzen (siehe unten).

**4.** Unter **Wie lange** wählen: **bis ich sie ausschalte**, **4 Stunden**, **8 Stunden** oder **bis Datum und Uhrzeit** – auch Wochen oder Monate im Voraus, zum Beispiel für den Urlaub.

**5.** **„Übernehmen“** tippen. Der Button wird grün.

Zum Ausschalten den Button antippen, **„Aus“** wählen und **„Übernehmen“**. Mit gewählter Dauer schaltet sich die Ansage zur eingestellten Zeit von selbst aus.

| Satz | Wann passend |
| --- | --- |
| Nicht zu Hause – andere Etage | Besucher soll es bei einer anderen Klingel versuchen |
| Nicht da – am Handy erreichbar | Besucher soll anrufen |

## Was beim Klingeln passiert

| Einstellung | Ablauf |
| --- | --- |
| Normal | Das Tablet klingelt wie immer. Nimmt nach 30 Sekunden niemand ab, spielt die Türstation den Satz ab. |
| Sofort ansagen | Das Tablet klingelt **nicht**. Die Türstation spielt den Satz nach einer Sekunde ab. Das Tablet zeigt den Besucher stumm mit Bild; mit **„Sprechen“** kann man trotzdem antworten. |

> **„Ende“ heißt: keine Ansage.** Wer „Ende“ tippt, hat den Besucher gesehen und will nicht öffnen – dann bleibt die Türstation still. „Sprechen“ während der Ansage bricht sie sofort ab.

Tablet und Handy derselben Etage zeigen immer dieselbe Einstellung; sie liegt in der Türstation. Am Handy geht das Umschalten nur im Haus-WLAN.

## Verpasste Besuche

Wird ein Ruf nicht angenommen, merkt sich das Tablet den Besuch mit **Datum, Uhrzeit und Bild von der Tür**. Unter dem Etagennamen erscheint dann **„1 verpasster Besuch“** (orange). Antippen öffnet die Liste; ein Bild antippen vergrößert es. Einzelne Besuche mit **„Löschen“** entfernen oder alle mit **„Alle löschen“**. Gespeichert werden die letzten 30 Besuche.

---

# Zahnrad: Empfang prüfen und Klingelton

Im Bereitschaftsbildschirm auf das **Zahnrad rechts oben** tippen, ohne PIN. Das Zahnrad erscheint auf allen drei Tablets, aber nicht während eines Rufs. Es öffnet ein kleines Menü:

| Menüpunkt | Zweck |
| --- | --- |
| WLAN-Empfang anzeigen | Signalstärke, Stations-IP und Diagnoseseite der Türstation. |
| Originalton wiederherstellen | Setzt den Klingelton auf den ursprünglichen Ton der App zurück und spielt ihn kurz vor. |
| Lautstärke der Türstation | Öffnet die Lautstärkeseite der Türstation (Admin-Anmeldung, siehe „Zugangsdaten“). Von dort geht es auch zu „Ansagen bei Abwesenheit“. | <!-- nur admin -->
| Lautstärke der Türstation | Nur für den Verwalter, mit Passwort geschützt. | <!-- nur bewohner -->

Einen anderen Klingelton wählt man in den Android-Einstellungen: Fully mit der Kiosk-PIN verlassen (7-mal schnell tippen), dann **Einstellungen → Apps und Benachrichtigungen → Open Intercom → Benachrichtigungen → Türklingel – (eigene Etage) → Ton**. Danach Fully wieder starten (**START KIOSK MODE → YES**).

## Empfangsanzeige und Stations-IP

„WLAN-Empfang anzeigen“ liest den Empfang **an der Türstation**, nicht den Empfang des Tablets. Der Wert wird alle zwei Sekunden aktualisiert. Je näher der negative dBm-Wert an 0 liegt, desto stärker ist das Signal.

| Ampel | Messwert | Bedeutung |
| --- | --- | --- |
| Grün | ab -65 dBm | Gut bis sehr gut |
| Gelb | -66 bis -75 dBm | Gerade noch ausreichend |
| Rot | unter -75 dBm | Zu schwach |
| Grau | Kein aktueller Messwert | Verbindung oder Messung fehlt |

Die IP-Adresse und die zugehörige Diagnoseadresse stehen unter dem Messwert. Die Adresse lässt sich markieren. **„Diagnoseseite öffnen“** zeigt die Diagnose direkt in der App. „Zurück“ führt zur Empfangsanzeige, „Schließen“ zur Gegensprechanlage.

> Ohne aktuelle Antwort wird die IP als **letzte bekannte Adresse** gekennzeichnet. Eine graue Ampel ist kein Beweis für schwachen Empfang: Die Türstation kann auch im anderen WLAN oder ausgeschaltet sein.

Die IP kann sich nach einem Router- oder Netzwechsel ändern. Verwende deshalb bevorzugt die aktuell angezeigte Adresse statt eines alten Lesezeichens.


<!-- admin -->
---

# Lautstärke und Zugangsdaten

Für die Wartung durch den Betreiber

## Zugangsdaten

Die Seiten **/audio** (Lautstärke) und **/ansage** (Ansagen) sind geschützt. Anmeldung:

| Zugang | Wert |
| --- | --- |
| Benutzer | **admin** |
| Passwort | **‹Admin-Passwort›** |

Die Anmeldung funktioniert nur aus **Haus-WLAN**. Wird das Passwort nicht angenommen, obwohl es stimmt, die Seite in einem privaten Browserfenster öffnen; Browser merken sich alte Anmeldedaten.

## Lautstärke einstellen

Zu leise oder zu laut wird an der **Türstation** eingestellt, nicht am Tablet (dort steht die Medienlautstärke schon auf Maximum).

**1.** In Haus-WLAN **http://STATIONS-IP/audio** öffnen oder auf der Diagnoseseite **„Lautstärke“** wählen. Am Tablet geht es auch über das **Zahnrad → Lautstärke der Türstation**. Anmeldung siehe „Zugangsdaten“.

**2.** Zwei Schieberegler:

| Regler | Wirkt auf |
| --- | --- |
| Türton am Tablet/Handy | Mikrofon der Türstation – wie laut man die Person vor der Tür hört |
| Stimme an der Tür | Lautsprecher der Türstation – wie laut man draußen den Sprecher am Tablet hört |

**3.** **Speichern**. Die Werte gelten sofort und bleiben nach einem Neustart erhalten. Die Regler reichen von **0,5-fach** (halb so laut) bis **8-fach** (achtmal so laut). **1,0-fach** heißt: Der Ton bleibt so, wie er ist. Voreinstellung: Türton 4,0-fach, Stimme an der Tür 1,0-fach.

## Ansagen verwalten

Die Sätze liegen in der **Türstation** und gelten für alle Parteien. Die Bewohner wählen nur aus; ändern kann die Sammlung nur der Verwalter.

**1.** In Haus-WLAN **http://STATIONS-IP/ansage** öffnen (Anmeldung siehe „Zugangsdaten“). Am Tablet: **Zahnrad → Lautstärke der Türstation → Ansagen bei Abwesenheit**.

**2.** Die Seite zeigt:

| Bereich | Inhalt |
| --- | --- |
| Auswahl der Parteien | Was jede Etage eingestellt hat, mit **„Ausschalten“** |
| Sätze | Jeder Satz mit Länge und Qualität: **Hier anhören**, **An der Tür abspielen**, **Löschen** |
| Neuer Satz | Name, Audiodatei (WAV, MP3, M4A …, höchstens 20 Sekunden), Qualität, freier Speicher |
| Einstellungen | **Wartezeit** bis zur Ansage (10–120 s, Standard 30) und **Lautstärke** der Ansage (Standard 80 %) |

**3.** Sätze **neutral** halten, ohne Namen und ohne Etage – sie gelten für alle. Kurz ist besser: 3 bis 6 Sekunden. Platz ist für etwa 8 Sätze, zusammen rund 17 Sekunden in CD-Qualität.

> Wird ein Satz gelöscht, steht jede Partei, die ihn gewählt hatte, danach auf „aus“. Für einen Probelauf an der Tür **„An der Tür abspielen“** verwenden – das geht nur, wenn es gerade nicht klingelt.

Vorbereiten lassen sich die Dateien zum Beispiel mit Audacity: Stille am Anfang und Ende wegschneiden, normalisieren, als WAV oder MP3 exportieren. Die Seite wandelt die Datei selbst um.

## USB-Befehle für die Wartung

Ist die Türstation per USB am PC (serieller Port, 115200 Baud), helfen diese Befehle ohne WLAN:

| Befehl | Wirkung |
| --- | --- |
| ANSAGE_LIST | Sätze, Auswahl jeder Etage, freier Speicher, ob die Uhr gestellt ist |
| ANSAGE_PLAY 1 | Satz 1 an der Tür abspielen |
| SPEAKER_TONE | Eine Sekunde Testton – prüft Lautsprecher und Verstärker |

> **Stromversorgung:** Die Platine der Türstation bekommt **5 V**, nie die 12 V vom Netzteil. 12 V zerstören den Verstärker MAX98357A und die Kamera-Platine (XIAO).
<!-- /admin -->
