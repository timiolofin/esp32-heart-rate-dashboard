#pragma once

#include <WiFi.h>
#include <HTTPClient.h>

static const char* API_URL = "http://192.168.0.17:8000/data";
static const char* API_TOKEN = "test123";

bool sendHeartRateData(const String& deviceId, int bpm, bool fingerDetected, long signal) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("HTTP skipped: Wi-Fi not connected");
    return false;
  }

  HTTPClient http;
  http.begin(API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_TOKEN);

  String json = "{";
  json += "\"device_id\":\"" + deviceId + "\",";
  json += "\"bpm\":" + String(bpm) + ",";
  json += "\"finger_detected\":" + String(fingerDetected ? "true" : "false") + ",";
  json += "\"signal\":" + String(signal);
  json += "}";

  int httpCode = http.POST(json);
  String response = http.getString();

  Serial.print("HTTP code: ");
  Serial.println(httpCode);
  Serial.print("Response: ");
  Serial.println(response);

  http.end();
  return httpCode > 0 && httpCode < 300;
}