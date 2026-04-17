#pragma once

#include <WiFi.h>
#include "secrets.h"

struct WiFiNetwork {
  const char* ssid;
  const char* password;
};

static WiFiNetwork networks[] = {
  {WIFI_SSID_1, WIFI_PASS_1},
  {WIFI_SSID_2, WIFI_PASS_2},
  {WIFI_SSID_3, WIFI_PASS_3}
};

static const int NUM_NETWORKS = sizeof(networks) / sizeof(networks[0]);

bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(500);

  Serial.print("ESP32 STA MAC: ");
  Serial.println(WiFi.macAddress());

  for (int i = 0; i < NUM_NETWORKS; i++) {
    if (!networks[i].ssid || strlen(networks[i].ssid) == 0) continue;

    Serial.print("Trying Wi-Fi: ");
    Serial.println(networks[i].ssid);

    WiFi.begin(networks[i].ssid, networks[i].password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWi-Fi connected.");
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }

    Serial.println("\nFailed. Moving to next network...");
    WiFi.disconnect(true, true);
    delay(500);
  }

  Serial.println("No Wi-Fi connected.");
  return false;
}