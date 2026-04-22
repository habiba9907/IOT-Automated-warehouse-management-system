#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <ESP32Servo.h>
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include "firebase_config.h"
#include "sensors.h"
#include "belt.h"
#include "gates.h"

#define WIFI_SSID "WE2025"
#define WIFI_PASSWORD "NMY2004#"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo gateServo;

// ---------------- STATES & TIMERS ----------------
bool gateMotorRunning = false;
bool servoRunning = false;
bool beltStoppedByIR = false;
bool fireActive = false;
bool fireClearedShown = false;

unsigned long servoStartTime = 0;
unsigned long gateMotorStartTime = 0;
unsigned long lastDHTRead = 0;
unsigned long irStopStartTime = 0;
unsigned long fireClearedTime = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastIRFirebaseUpdate = 0;

int lastDoorStatus = 0;
int lastWarehouseDoorStatus = 0;
int lastBeltCmd = 1;

const unsigned long WIFI_CHECK_INTERVAL = 10000;

void tokenStatusCallback(TokenInfo info) {
  Serial.println("Token status changed");
}

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.print("System Starting");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase SignUp OK");
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  fbdo.setBSSLBufferSize(4096, 1024);

  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  pinMode(BELT_IN1, OUTPUT);
  pinMode(BELT_IN2, OUTPUT);
  pinMode(G2_1, OUTPUT);
  pinMode(G2_2, OUTPUT);

  pinMode(IR_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  dht.begin();

  conveyorBeltMotorForward();

  digitalWrite(G2_1, HIGH);
  digitalWrite(G2_2, LOW);

  lcd.clear();
  lcd.print("System Ready");
}

void loop() {

  // ---------------- IR (HIGHEST PRIORITY) ----------------
  handleIR();

  // ---------------- WIFI RECONNECT ----------------
  if (millis() - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.reconnect();
      unsigned long reconnectStart = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - reconnectStart < 10000) {
        delay(300);
      }
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi reconnected");
      } else {
        Serial.println("WiFi reconnect failed");
      }
    }
  }

  // ---------------- SENSORS ----------------
  handleFlame();
  handleDHT();
  handleAlarm();

  // ---------------- BELT ----------------
  handleBeltFirebase(lastBeltCmd);

  // ---------------- GATES ----------------
  handleGateMotor();
  handleServo();
}