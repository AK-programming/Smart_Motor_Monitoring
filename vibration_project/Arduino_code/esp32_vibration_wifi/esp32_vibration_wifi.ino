#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ========== WIFI ==========
const char* ssid = "IOT";
const char* password = "12345678";

// ========== SERVER ==========
WebServer server(80);

// ========== PINS ==========
#define DHTPIN 4
#define DHTTYPE DHT22

#define SOIL_PIN 34
#define FAN_PIN 18
#define PUMP_PIN 19
#define SERVO_PIN 5

// ========== OBJECTS ==========
DHT dht(DHTPIN, DHTTYPE);
Servo ventServo;

// ========== THRESHOLDS ==========
#define FAN_TEMP 28
#define VENT_TEMP 30
#define SOIL_THRESHOLD 2000

// ========== VARIABLES ==========
float temp, hum;
int soil;
bool fanState, pumpState;
int ventPos;

// ========== HTML DASHBOARD ==========
String webpage = "";

void handleRoot() {
  webpage = "<html><head><title>Greenhouse</title></head><body>";
  webpage += "<h2>Smart Greenhouse Dashboard</h2>";

  webpage += "<p>Temperature: " + String(temp) + " C</p>";
  webpage += "<p>Humidity: " + String(hum) + " %</p>";
  webpage += "<p>Soil: " + String(soil) + "</p>";

  webpage += "<p>Fan: " + String(fanState ? "ON" : "OFF") + "</p>";
  webpage += "<p>Pump: " + String(pumpState ? "ON" : "OFF") + "</p>";
  webpage += "<p>Vent: " + String(ventPos == 90 ? "OPEN" : "CLOSED") + "</p>";

  webpage += "<br><a href='/fanon'>Fan ON</a>";
  webpage += "<br><a href='/fanoff'>Fan OFF</a>";
  webpage += "<br><a href='/pumpon'>Pump ON</a>";
  webpage += "<br><a href='/pumpoff'>Pump OFF</a>";

  webpage += "</body></html>";

  server.send(200, "text/html", webpage);
}

// ========== MANUAL CONTROL ==========
void fanOn(){ digitalWrite(FAN_PIN, LOW); }
void fanOff(){ digitalWrite(FAN_PIN, HIGH); }
void pumpOn(){ digitalWrite(PUMP_PIN, LOW); }
void pumpOff(){ digitalWrite(PUMP_PIN, HIGH); }

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);

  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);

  digitalWrite(FAN_PIN, HIGH);
  digitalWrite(PUMP_PIN, HIGH);

  dht.begin();

  ventServo.attach(SERVO_PIN);
  ventServo.write(0);

  // WiFi connect
  WiFi.begin(ssid, password);
  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  // Server routes
  server.on("/", handleRoot);
  server.on("/fanon", fanOn);
  server.on("/fanoff", fanOff);
  server.on("/pumpon", pumpOn);
  server.on("/pumpoff", pumpOff);

  server.begin();
}

// ========== LOOP ==========
void loop() {

  server.handleClient();

  temp = dht.readTemperature();
  hum  = dht.readHumidity();
  soil = analogRead(SOIL_PIN);

  if (isnan(temp) || isnan(hum)) return;

  // FAN AUTO
  if (temp >= FAN_TEMP) {
    digitalWrite(FAN_PIN, LOW);
    fanState = true;
  } else {
    digitalWrite(FAN_PIN, HIGH);
    fanState = false;
  }

  // VENT AUTO
  if (temp >= VENT_TEMP) {
    ventServo.write(90);
    ventPos = 90;
  } else {
    ventServo.write(0);
    ventPos = 0;
  }

  // PUMP AUTO
  if (soil > SOIL_THRESHOLD) {
    digitalWrite(PUMP_PIN, LOW);
    pumpState = true;
  } else {
    digitalWrite(PUMP_PIN, HIGH);
    pumpState = false;
  }

  delay(1500);
}