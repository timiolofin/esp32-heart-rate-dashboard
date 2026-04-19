// =============================================================================
// http_helper.h
// Handles HTTP POST transmission of sensor readings to the VitalSense backend.
// Builds a JSON payload from the processed vital sign data and sends it to
// the Render-hosted FastAPI endpoint with Bearer token authentication.
// =============================================================================

#pragma once

#include <WiFi.h>
#include <HTTPClient.h>

static const char* API_URL   = "https://esp32-heart-rate-dashboard.onrender.com/data";
static const char* API_TOKEN = "test123";

// =============================================================================
// sendVitalData()
// Constructs and sends a single JSON reading to the backend.
// Returns true on HTTP 2xx, false on network failure or error response.
// Skips transmission entirely if Wi-Fi is not connected.
// =============================================================================
bool sendVitalData(
  const String& deviceId,
  const String& sessionId,
  int   heartRate,
  bool  hrValid,
  float spo2,
  bool  spo2Valid,
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

  // Build JSON payload manually — avoids pulling in a JSON library
  String json = "{";
  json += "\"device_id\":\""   + deviceId                          + "\",";
  json += "\"session_id\":\""  + sessionId                         + "\",";
  json += "\"heart_rate\":"    + String(heartRate)                 + ",";
  json += "\"hr_valid\":"      + String(hrValid ? "true" : "false")+ ",";
  json += "\"spo2\":"          + String(spo2, 1)                   + ",";
  json += "\"spo2_valid\":"    + String(spo2Valid ? "true" : "false") + ",";
  json += "\"ir\":"            + String(irValue)                   + ",";
  json += "\"red\":"           + String(redValue)                  + ",";
  json += "\"ratio\":"         + String(ratio, 4)                  + ",";
  json += "\"correlation\":"   + String(correl, 4);
  json += "}";

  Serial.println("Sending JSON:");
  Serial.println(json);

  int    httpCode = http.POST(json);
  String response = http.getString();

  Serial.print("HTTP code: ");
  Serial.println(httpCode);
  Serial.print("Response: ");
  Serial.println(response);

  http.end();
  return httpCode > 0 && httpCode < 300;
}