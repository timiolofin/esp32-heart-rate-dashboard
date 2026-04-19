// =============================================================================
// wifi_helper.h
// Handles Wi-Fi connection for the ESP32-S3.
// Tries multiple known networks in order until one connects.
// Credentials are loaded from secrets.h which is excluded from version control.
// =============================================================================

#pragma once

#include <WiFi.h>
#include "secrets.h"

// Struct to hold a single Wi-Fi network's credentials
struct WiFiNetwork {
  const char* ssid;
  const char* password;
};

// List of known networks to try — defined in secrets.h
// Add more networks here if needed (e.g. home, school, hotspot)
static WiFiNetwork networks[] = {
  {WIFI_SSID_1, WIFI_PASS_1},
  {WIFI_SSID_2, WIFI_PASS_2},
  {WIFI_SSID_3, WIFI_PASS_3}
};

static const int NUM_NETWORKS = sizeof(networks) / sizeof(networks[0]);

// =============================================================================
// connectToWiFi()
// Iterates through the known networks list and attempts to connect to each one.
// Tries each network for up to 10 seconds (20 attempts x 500ms).
// Returns true if a connection is established, false if all networks fail.
// The ESP32 will continue without Wi-Fi if this returns false — readings
// will still be computed locally but not transmitted to the backend.
// =============================================================================
bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(500);

  Serial.print("ESP32 STA MAC: ");
  Serial.println(WiFi.macAddress());

  for (int i = 0; i < NUM_NETWORKS; i++) {
    // Skip empty entries
    if (!networks[i].ssid || strlen(networks[i].ssid) == 0) continue;

    Serial.print("Trying Wi-Fi: ");
    Serial.println(networks[i].ssid);

    WiFi.begin(networks[i].ssid, networks[i].password);

    // Poll connection status — 20 attempts at 500ms each = 10 second timeout
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

    // This network failed — disconnect cleanly before trying the next
    Serial.println("\nFailed. Moving to next network...");
    WiFi.disconnect(true, true);
    delay(500);
  }

  Serial.println("No Wi-Fi connected.");
  return false;
}