/*
 * ESP32-S3 Vibration Monitoring System - WiFi Version
 * Collects vibration data from MPU6050 and sends to Python server via WiFi
 * 
 * Hardware: ESP32-S3 Dev Module + MPU6050
 */

#include <Wire.h>
#include <MPU6050.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============ CONFIGURATION - CHANGE THESE! ============
const char* WIFI_SSID = "IOT";           // Your WiFi network name
const char* WIFI_PASSWORD = "12345678";    // Your WiFi password
const char* SERVER_URL = "http://192.168.43.95:5000/predict";  // Your computer's IP address

// ============ SENSOR SETTINGS ============
MPU6050 mpu;
const int WINDOW_SIZE = 512;      // Number of samples to collect (must match training)
const int SAMPLE_RATE_MS = 5;     // 5ms = 200Hz sampling rate

// Data buffers
float accel_x[WINDOW_SIZE];
float accel_y[WINDOW_SIZE];
float accel_z[WINDOW_SIZE];
int sample_count = 0;

// Status LED
const int LED_PIN = 2;  // Built-in LED on ESP32-S3

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32-S3 Vibration Monitor v1.0");
  Serial.println("=================================\n");
  
  // Initialize I2C and MPU6050
  Serial.println("1. Initializing MPU6050 sensor...");
  Wire.begin(8, 9);  // ESP32-S3: SDA=GPIO8, SCL=GPIO9 (change if needed)

  // Verify that the MPU6050 is present on I2C
  Wire.beginTransmission(0x68);
  if (Wire.endTransmission() != 0) {
    Serial.println("   ✗ MPU6050 not detected on I2C. Check wiring and power.");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }

  mpu.begin();
  mpu.calcGyroOffsets();
  Serial.println("   ✓ MPU6050 ready!");
  
  // Connect to WiFi
  Serial.println("\n2. Connecting to WiFi...");
  Serial.print("   Network: ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // Blink while connecting
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n   ✓ WiFi Connected!");
    Serial.print("   IP Address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(LED_PIN, HIGH);  // Solid LED when connected
  } else {
    Serial.println("\n   ✗ WiFi Connection FAILED!");
    Serial.println("   Please check SSID and password");
    while(1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);  // Fast blink = error
    }
  }
  
  Serial.println("\n3. System ready! Starting vibration monitoring...\n");
  delay(2000);
}

void loop() {
  // Collect vibration samples
  if (sample_count < WINDOW_SIZE) {
    mpu.update();
    accel_x[sample_count] = mpu.getAccX();
    accel_y[sample_count] = mpu.getAccY();
    accel_z[sample_count] = mpu.getAccZ();
    sample_count++;
    
    // Show progress every 100 samples
    if (sample_count % 100 == 0) {
      Serial.print("Collecting: ");
      Serial.print(sample_count);
      Serial.print("/");
      Serial.println(WINDOW_SIZE);
    }
    
    delay(SAMPLE_RATE_MS);
  } 
  else {
    // We have a complete window - send to server
    Serial.println("\n📊 Sending data to server...");
    digitalWrite(LED_PIN, LOW);  // LED off while sending
    
    bool success = sendDataToServer();
    
    if (success) {
      Serial.println("✓ Data sent successfully!\n");
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.println("✗ Failed to send data\n");
      // Blink 3 times to indicate error
      for(int i=0; i<3; i++) {
        digitalWrite(LED_PIN, LOW);
        delay(200);
        digitalWrite(LED_PIN, HIGH);
        delay(200);
      }
    }
    
    // Reset and wait before next reading
    sample_count = 0;
    delay(2000);  // Wait 2 seconds between predictions
  }
}

bool sendDataToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return false;
  }
  
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000);  // 10 second timeout
  
  // Build JSON payload
  // Format: {"accel_x": [1.2, 3.4, ...], "accel_y": [...], "accel_z": [...]}
  String payload = "{";
  
  // Add X-axis data
  payload += "\"accel_x\":[";
  for (int i = 0; i < WINDOW_SIZE; i++) {
    payload += String(accel_x[i], 4);  // 4 decimal places
    if (i < WINDOW_SIZE - 1) payload += ",";
  }
  payload += "],";
  
  // Add Y-axis data
  payload += "\"accel_y\":[";
  for (int i = 0; i < WINDOW_SIZE; i++) {
    payload += String(accel_y[i], 4);
    if (i < WINDOW_SIZE - 1) payload += ",";
  }
  payload += "],";
  
  // Add Z-axis data
  payload += "\"accel_z\":[";
  for (int i = 0; i < WINDOW_SIZE; i++) {
    payload += String(accel_z[i], 4);
    if (i < WINDOW_SIZE - 1) payload += ",";
  }
  payload += "]}";
  
  // Send POST request
  int httpCode = http.POST(payload);
  
  // Check response
  if (httpCode == 200) {
    String response = http.getString();
    Serial.println("Server Response:");
    Serial.println(response);
    http.end();
    return true;
  } else {
    Serial.print("HTTP Error: ");
    Serial.println(httpCode);
    if (httpCode > 0) {
      Serial.println(http.getString());
    }
    http.end();
    return false;
  }
}
