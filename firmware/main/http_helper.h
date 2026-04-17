#pragma once

#include <WiFi.h>
#include <HTTPClient.h>

static const char* API_URL   = "http://192.168.0.17:8000/data";
static const char* API_TOKEN = "test123";

bool sendVitalData(
  const String& deviceId,
  int heartRate,
  bool hrValid,
  float spo2,
  bool spo2Valid,
  uint32_t irValue,
  uint32_t redValue,
  float ratio,
  float correl
) {
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
  json += "\"heart_rate\":" + String(heartRate) + ",";
  json += "\"hr_valid\":" + String(hrValid ? "true" : "false") + ",";
  json += "\"spo2\":" + String(spo2, 1) + ",";
  json += "\"spo2_valid\":" + String(spo2Valid ? "true" : "false") + ",";
  json += "\"ir\":" + String(irValue) + ",";
  json += "\"red\":" + String(redValue) + ",";
  json += "\"ratio\":" + String(ratio, 4) + ",";
  json += "\"correlation\":" + String(correl, 4);
  json += "}";

  Serial.println("Sending JSON:");
  Serial.println(json);

  int httpCode = http.POST(json);
  String response = http.getString();

  Serial.print("HTTP code: ");
  Serial.println(httpCode);
  Serial.print("Response: ");
  Serial.println(response);

  http.end();
  return httpCode > 0 && httpCode < 300;
}