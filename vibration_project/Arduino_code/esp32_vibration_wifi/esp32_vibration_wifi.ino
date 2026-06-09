/*
 * ESP32-S3 Vibration Monitoring System - WiFi Version
 * Collects vibration data from MPU6050 and sends to cloud server.
 *
 * Hardware: ESP32-S3 Dev Module + MPU6050
 */

#include <Wire.h>
#include <MPU6050_tockn.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============ CONFIGURATION ============
const char* WIFI_SSID     = "IOT";
const char* WIFI_PASSWORD = "12345678";

// Replace with your Koyeb URL after deployment, e.g.:
// https://your-app-name-your-org.koyeb.app/predict
const char* SERVER_URL = "https://YOUR-APP.koyeb.app/predict";

// ============ SENSOR SETTINGS ============
MPU6050 mpu(Wire);
const int WINDOW_SIZE    = 512;   // Must match training
const int SAMPLE_RATE_MS = 5;     // 5ms = 200Hz

float accel_x[WINDOW_SIZE];
float accel_y[WINDOW_SIZE];
float accel_z[WINDOW_SIZE];
int sample_count = 0;

const int LED_PIN = 2;

void connectWiFi() {
  Serial.println("\n[WiFi] Connecting...");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_PIN, HIGH);
  } else {
    Serial.println("\n[WiFi] FAILED — restarting...");
    delay(3000);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  Serial.println("\n=================================");
  Serial.println(" ESP32-S3 Vibration Monitor v2.0");
  Serial.println("=================================");

  Serial.println("\n[Sensor] Initializing MPU6050...");
  Wire.begin(21, 22);
  Wire.setClock(400000);
  mpu.begin();
  mpu.calcGyroOffsets(true);
  Serial.println("[Sensor] MPU6050 ready!");

  connectWiFi();
  Serial.println("\n[System] Ready. Starting monitoring...\n");
  delay(500);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Reconnecting...");
    connectWiFi();
  }

  if (sample_count < WINDOW_SIZE) {
    mpu.update();
    accel_x[sample_count] = mpu.getAccX();
    accel_y[sample_count] = mpu.getAccY();
    accel_z[sample_count] = mpu.getAccZ();
    sample_count++;

    if (sample_count % 128 == 0) {
      Serial.printf("[Collect] %d / %d\n", sample_count, WINDOW_SIZE);
    }

    delay(SAMPLE_RATE_MS);
  } else {
    Serial.println("\n[Send] Window complete. Building payload...");
    digitalWrite(LED_PIN, LOW);

    String payload;
    payload.reserve(WINDOW_SIZE * 3 * 11 + 64);

    payload = "{\"accel_x\":[";
    for (int i = 0; i < WINDOW_SIZE; i++) {
      payload += String(accel_x[i], 4);
      if (i < WINDOW_SIZE - 1) payload += ",";
    }
    payload += "],\"accel_y\":[";
    for (int i = 0; i < WINDOW_SIZE; i++) {
      payload += String(accel_y[i], 4);
      if (i < WINDOW_SIZE - 1) payload += ",";
    }
    payload += "],\"accel_z\":[";
    for (int i = 0; i < WINDOW_SIZE; i++) {
      payload += String(accel_z[i], 4);
      if (i < WINDOW_SIZE - 1) payload += ",";
    }
    payload += "]}";

    Serial.printf("[Send] Payload size: %d bytes\n", payload.length());

    bool serverOK = sendPost(SERVER_URL, payload, "PredictionServer");

    if (serverOK) {
      Serial.println("[Status] Server received data.\n");
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.println("[Status] Server request failed.");
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, LOW);  delay(150);
        digitalWrite(LED_PIN, HIGH); delay(150);
      }
    }

    sample_count = 0;
    delay(1000);
  }
}

bool sendPost(const char* url, const String& payload, const char* label) {
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Connection", "keep-alive");
  http.setTimeout(30000);

  Serial.printf("[%s] POST -> %s\n", label, url);
  int httpCode = http.POST(payload);

  if (httpCode == 200) {
    String response = http.getString();
    Serial.printf("[%s] 200 OK | Response: %s\n", label, response.c_str());
    http.end();
    return true;
  }

  Serial.printf("[%s] HTTP %d\n", label, httpCode);
  if (httpCode > 0) {
    Serial.printf("[%s] Body: %s\n", label, http.getString().c_str());
  }
  http.end();
  return false;
}
