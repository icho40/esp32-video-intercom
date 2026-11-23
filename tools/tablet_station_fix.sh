#!/usr/bin/env bash
# Traegt auf per USB angeschlossenen Fire-Tablets die aktuelle Kennung und IP
# der Tuerstation in die Open-Intercom-App ein (door_station_id, door_host).
# PIN und Klingelzuordnung bleiben unveraendert; die alte Datei wird gesichert.
#
# Aufruf: tools/tablet_station_fix.sh [STATIONS-IP]   (Standard 192.168.179.2)
set -euo pipefail

STATION_IP="${1:-192.168.179.2}"
PKG=de.willi.openintercom
PREFS=shared_prefs/intercom_settings.xml

# Kennung direkt bei der Station abfragen (UDP 5004), nicht raten.
STATION_ID=$(python3 -I -c "
import socket,sys
s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.settimeout(3)
s.sendto(b'DISCOVER_OPEN_INTERCOM_V2',(sys.argv[1],5004))
r=s.recv(128).decode().strip()
assert r.startswith('OPEN_INTERCOM_V2 '), r
print(r.split(' ',1)[1])" "$STATION_IP") || { echo "Tuerstation unter $STATION_IP antwortet nicht." >&2; exit 1; }
echo "Tuerstation $STATION_IP meldet Kennung $STATION_ID"

found=0
for serial in $(adb devices | awk 'NR>1 && $2=="device" && $1 !~ /^emulator-/ {print $1}'); do
  found=1
  A=(adb -s "$serial")
  if ! "${A[@]}" shell run-as "$PKG" test -f "$PREFS" 2>/dev/null; then
    echo "[$serial] keine App-Einstellungen gefunden, uebersprungen"; continue
  fi
  before=$("${A[@]}" shell run-as "$PKG" cat "$PREFS" | grep -E 'door_station_id|door_host' | tr -s ' ' || true)
  if grep -q "$STATION_ID" <<<"$before" && grep -q ">$STATION_IP<" <<<"$before"; then
    echo "[$serial] bereits korrekt"; continue
  fi
  stamp=$(date +%Y%m%d-%H%M%S)
  "${A[@]}" shell run-as "$PKG" cp "$PREFS" "$PREFS.bak-$stamp"
  "${A[@]}" shell am force-stop "$PKG"
  "${A[@]}" shell "run-as $PKG sed -i \
    -e 's#<string name=\"door_host\">[^<]*</string>#<string name=\"door_host\">$STATION_IP</string>#' \
    -e 's#<string name=\"door_station_id\">[^<]*</string>#<string name=\"door_station_id\">$STATION_ID</string>#' \
    $PREFS"
  # Fehlende Eintraege ergaenzen (z. B. nie eingerichtete Station).
  "${A[@]}" shell "run-as $PKG grep -q door_station_id $PREFS || run-as $PKG sed -i 's#</map>#    <string name=\"door_station_id\">$STATION_ID</string>\n</map>#' $PREFS"
  "${A[@]}" shell "run-as $PKG grep -q door_host $PREFS || run-as $PKG sed -i 's#</map>#    <string name=\"door_host\">$STATION_IP</string>\n</map>#' $PREFS"
  "${A[@]}" shell monkey -p "$PKG" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
  echo "[$serial] vorher: $before"
  echo "[$serial] jetzt:  $("${A[@]}" shell run-as "$PKG" cat "$PREFS" | grep -E 'door_station_id|door_host' | tr -s ' ')"
  echo "[$serial] Sicherung: $PREFS.bak-$stamp"
done
[ "$found" = 1 ] || { echo "Kein Tablet per USB gefunden." >&2; exit 1; }

echo "Warte 40 s auf Anmeldung ..."
sleep 40
curl -s -m 5 "http://$STATION_IP/api/diagnose" | python3 -I -c \
  "import json,sys; print('Angemeldete Tablets:', json.load(sys.stdin)['tablets'])"
