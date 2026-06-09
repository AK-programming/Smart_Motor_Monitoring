/*
 * ESP32-S3 Vibration Monitoring System - WiFi Version
 * Collects vibration data from MPU6050 and sends to Hugging Face server.
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

// Hugging Face Space (no trailing slash)
const char* SERVER_BASE   = "https://afnan2155-motor-health-monitor.hf.space";
const char* HEALTH_PATH   = "/health";
const char* PREDICT_PATH  = "/predict";

// Hugging Face cold starts can take 60-90s when the Space was sleeping
const int HTTP_TIMEOUT_MS     = 90000;
const int WAKE_MAX_ATTEMPTS   = 6;
const int WAKE_RETRY_DELAY_MS = 10000;
const int POST_MAX_RETRIES    = 3;

// ============ SENSOR SETTINGS ============
MPU6050 mpu(Wire);
const int WINDOW_SIZE    = 512;   // Must match training
const int SAMPLE_RATE_MS = 5;     // 5ms = 200Hz

float accel_x[WINDOW_SIZE];
float accel_y[WINDOW_SIZE];
float accel_z[WINDOW_SIZE];
int sample_count = 0;

const int LED_PIN = 2;

String buildUrl(const char* path) {
  return String(SERVER_BASE) + String(path);
}

bool isRetryableHttpCode(int code) {
  return code < 0 || code == 500 || code == 502 || code == 503 || code == 504;
}

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

bool wakeServer() {
  String healthUrl = buildUrl(HEALTH_PATH);
  Serial.println("[Wake] Checking Hugging Face Space...");

  for (int attempt = 1; attempt <= WAKE_MAX_ATTEMPTS; attempt++) {
    HTTPClient http;
    http.begin(healthUrl);
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.addHeader("Connection", "close");

    Serial.printf("[Wake] GET %s (try %d/%d)\n", healthUrl.c_str(), attempt, WAKE_MAX_ATTEMPTS);
    int httpCode = http.GET();

    if (httpCode == 200) {
      Serial.printf("[Wake] Space is ready: %s\n", http.getString().c_str());
      http.end();
      return true;
    }

    Serial.printf("[Wake] HTTP %d", httpCode);
    if (httpCode > 0) {
      Serial.printf(" | %s", http.getString().c_str());
    }
    Serial.println();

    http.end();

    if (attempt < WAKE_MAX_ATTEMPTS) {
      Serial.printf("[Wake] Waiting %d s before retry...\n", WAKE_RETRY_DELAY_MS / 1000);
      delay(WAKE_RETRY_DELAY_MS);
    }
  }

  Serial.println("[Wake] Space did not become ready.");
  return false;
}

bool sendPost(const char* path, const String& payload, const char* label) {
  String url = buildUrl(path);

  for (int attempt = 1; attempt <= POST_MAX_RETRIES; attempt++) {
    if (!wakeServer()) {
      Serial.printf("[%s] Skipping POST — server not awake (try %d/%d)\n",
                    label, attempt, POST_MAX_RETRIES);
      if (attempt < POST_MAX_RETRIES) {
        delay(WAKE_RETRY_DELAY_MS);
        continue;
      }
      return false;
    }

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Connection", "close");
    http.setTimeout(HTTP_TIMEOUT_MS);

    Serial.printf("[%s] POST -> %s (try %d/%d)\n", label, url.c_str(), attempt, POST_MAX_RETRIES);
    int httpCode = http.POST(payload);

    if (httpCode == 200) {
      String response = http.getString();
      Serial.printf("[%s] 200 OK | Response: %s\n", label, response.c_str());
      http.end();
      return true;
    }

    Serial.printf("[%s] HTTP %d", label, httpCode);
    if (httpCode > 0) {
      Serial.printf(" | Body: %s", http.getString().c_str());
    }
    Serial.println();
    http.end();

    if (!isRetryableHttpCode(httpCode) || attempt == POST_MAX_RETRIES) {
      return false;
    }

    Serial.printf("[%s] Retrying in %d s...\n", label, WAKE_RETRY_DELAY_MS / 1000);
    delay(WAKE_RETRY_DELAY_MS);
  }

  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  Serial.println("\n=================================");
  Serial.println(" ESP32-S3 Vibration Monitor v2.1");
  Serial.println(" Hugging Face Cloud Edition");
  Serial.println("=================================");

  Serial.println("\n[Sensor] Initializing MPU6050...");
  Wire.begin(21, 22);
  Wire.setClock(400000);
  mpu.begin();
  mpu.calcGyroOffsets(true);
  Serial.println("[Sensor] MPU6050 ready!");

  connectWiFi();

  Serial.println("\n[System] Waking Hugging Face Space before monitoring...");
  wakeServer();

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

    bool serverOK = sendPost(PREDICT_PATH, payload, "HuggingFace");

    if (serverOK) {
      Serial.println("[Status] Prediction received.\n");
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.println("[Status] Prediction failed — Space may still be waking up.");
      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, LOW);  delay(150);
        digitalWrite(LED_PIN, HIGH); delay(150);
      }
    }

    sample_count = 0;
    delay(2000);
  }
}
