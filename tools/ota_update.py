import requests,json,re,http.client,time,sys
from pathlib import Path
if len(sys.argv)!=2:raise SystemExit("Aufruf: python3 tools/ota_update.py PFAD_ZUR_FIRMWARE.ino.bin")
firmware=Path(sys.argv[1])
if not firmware.is_file() or not 0<firmware.stat().st_size<=0x330000:raise SystemExit("Ungueltige Firmware-Dateigroesse")
from requests.auth import HTTPDigestAuth
from urllib.parse import urlsplit
c=json.loads((Path.home()/'.local/share/open-intercom/doorstation-ota.json').read_text())
# Stations-IP kann sich aendern: OTA_HOST=192.168.x.y ueberschreibt den Host aus der Zugangsdatei.
import os
if os.environ.get('OTA_HOST'):c['url']=urlsplit(c['url'])._replace(netloc=os.environ['OTA_HOST']).geturl()
s=requests.Session();auth=HTTPDigestAuth(c['user'],c['password']);s.auth=auth
r=s.get(c['url'],timeout=15);assert r.status_code==200
path=re.search(r"action='([^']+)'",r.text).group(1);u=urlsplit(c['url']);url='http://'+u.netloc+path
body=b'--intercomboundary\r\nContent-Disposition: form-data; name="firmware"; filename="doorstation_v2.ino.bin"\r\nContent-Type: application/octet-stream\r\n\r\n'+firmware.read_bytes()+b'\r\n--intercomboundary--\r\n'
h=http.client.HTTPConnection(u.hostname,timeout=45);h.putrequest('POST',path);h.putheader('Authorization',auth.build_digest_header('POST',url));h.putheader('Content-Type','multipart/form-data; boundary=intercomboundary');h.putheader('Content-Length',str(len(body)));h.endheaders()
try:
 for off in range(0,len(body),1024):
  h.send(body[off:off+1024]);time.sleep(.02)
 print('Upload sent',len(body),flush=True)
 r=h.getresponse();print('OTA response',r.status,r.read().decode(),flush=True)
 if r.status!=200:raise SystemExit('OTA fehlgeschlagen; laufende Firmware pruefen')
finally:h.close()
