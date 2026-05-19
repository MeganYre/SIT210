/*
  Wi-Fi and ThingSpeak settings for FinalProject_HealthMonitor.ino

  Edit this file only — do not share or upload to public Git repos.
*/

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// Wi-Fi (2.4 GHz network required for ESP8266)
//#define WIFI_SSID     "SE_Staff"
//#define WIFI_PASSWORD "mVuEqMR2vN"

#define WIFI_SSID     "dont mesh with me"
#define WIFI_PASSWORD "97081630"

// ThingSpeak channel (https://thingspeak.com/channels/3384179)
#define THINGSPEAK_CHANNEL_ID      3384179UL
#define THINGSPEAK_WRITE_API_KEY   "9MO0NY8NP6SOMXV5"   // Arduino uploads
#define THINGSPEAK_READ_API_KEY    "SW96ULW04V7B8TBU"   // web dashboard reads only

#endif
