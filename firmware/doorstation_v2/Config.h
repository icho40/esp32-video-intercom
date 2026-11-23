#pragma once
// Namen der beiden WLANs, zwischen denen die Station wählen kann.
// Die Zugangsdaten selbst liegen nicht hier, sondern im NVS der Station.
//  - HOME_WIFI_SSID: WLAN am Einsatzort. Nur dort ist das Firmware-Update (/update) erlaubt.
//                    Beim Start mit gedrückter P2 wird dieses WLAN gewählt.
//  - TEST_WIFI_SSID: zweites WLAN, zum Beispiel zum Testen zu Hause.
//                    Beim Start mit gedrückter P1 wird dieses WLAN gewählt.
#define HOME_WIFI_SSID "Haus-WLAN"
#define TEST_WIFI_SSID "Test-WLAN"
