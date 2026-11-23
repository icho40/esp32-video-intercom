# ESP32 Open Intercom V2

## Hardware

Die vorhandene Hardware und Platine werden nicht verändert.

### XIAO -> Türstations-ESP32

- XIAO ESP32-S3 Sense TX: GPIO43
- Türstations-ESP32 RX: GPIO18
- UART: 1.000.000 Baud
- 8 Datenbits
- keine Parität
- 1 Stopbit
- keine Flusskontrolle
- Verbindung ausschließlich XIAO -> ESP32

## UART-Paket

Binärer Header:

- type: uint8_t
- seq: uint16_t
- timestamp_ms: uint32_t
- payload_len: uint32_t

Alle Integerwerte werden Little Endian übertragen.

Der komplette Header mit Payload wird COBS-codiert.
Jedes codierte Paket endet mit 0x00.

## Pakettypen

- 0x01: JPEG-Kamerabild
- 0x02: PCM-Audio vom Türmikrofon

## Audioformat

- PCM signed 16 Bit
- Little Endian
- Mono
- 16000 Samples/s

## Zuständigkeiten

### XIAO ESP32-S3 Sense

Der XIAO ist ausschließlich Sensor-Frontend:

- Kamera aufnehmen
- JPEG-Pakete erzeugen
- PDM-Mikrofon aufnehmen
- PCM-Pakete erzeugen
- Daten über UART senden

Der XIAO kennt keine Klingeltaster, Parteien oder Gesprächszustände.

### Türstations-ESP32

Der Türstations-ESP32 ist der Master der Gegensprechanlage.

Er übernimmt:

- Klingeltaster P1/P2/P3
- Zustandsautomat
- Empfang von JPEG und PCM vom XIAO
- Kommunikation mit dem Fire Tablet
- Ausgabe von Tablet-Audio über MAX98357A

### Fire Tablet

Das Fire Tablet ist ein Client des Türstations-ESP32.

Es übernimmt:

- Anzeige des Kamerabildes
- Klingelton
- Wiedergabe des Türmikrofons
- Aufnahme des Tablet-Mikrofons
- Bedienung der Gegensprechanlage



## Netzwerk: Türstations-ESP32 <-> Fire Tablet

Die Kommunikation erfolgt ausschließlich im lokalen WLAN.
Es wird kein externer Server und keine Cloud benötigt.

Der Türstations-ESP32 ist Server.
Das Fire Tablet verbindet sich als Client.

### TCP-Verbindung

Eine dauerhafte TCP-Verbindung transportiert:

- Verbindungsstatus
- Klingelereignisse
- Gesprächssteuerung
- Kamerabilder

Steuernachrichten:

- STATE IDLE
- RING 1
- RING 2
- RING 3
- ANSWER
- HANGUP

Die endgültige binäre Codierung des TCP-Protokolls wird vor der
Implementierung festgelegt.

### Audio über UDP

Audio wird in beiden Richtungen als UDP übertragen.

Format:

- PCM signed 16 Bit
- Little Endian
- Mono
- 16000 Samples/s
- 20 ms Audio pro Paket
- 320 Samples pro Paket
- 640 Byte PCM-Payload

Jedes Audiopaket erhält mindestens:

- Richtung bzw. Typ
- Sequenznummer
- Zeitstempel

Tür -> Tablet:

XIAO-Mikrofon -> UART -> Türstations-ESP32 -> UDP -> Fire Tablet

Tablet -> Tür:

Fire-Mikrofon -> UDP -> Türstations-ESP32 -> I2S -> MAX98357A

Verlorene Audiopakete werden nicht erneut übertragen.

## Zustandsautomat

Der alleinige verbindliche Anlagenzustand liegt im Türstations-ESP32.

Grundzustände:

- IDLE
- RINGING_P1
- RINGING_P2
- RINGING_P3
- TALKING

Das Fire Tablet bildet diesen Zustand lediglich ab.

Nach einer Neuverbindung fragt das Tablet den aktuellen Zustand beim
Türstations-ESP32 ab bzw. erhält ihn vom ESP32 erneut.

Ein Neustart oder Verbindungsverlust des Tablets darf den Zustand des
Türstations-ESP32 nicht undefiniert machen.
