#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <ESP32Servo.h>
#include "DHT.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// wifi
#define WIFI_SSID "WE2025"
#define WIFI_PASSWORD "NMY2004#"

// firebase
#define API_KEY "AIzaSyCpp66md_gW7bpF6z2yP2l5dFDxH1RS1DM"
#define DATABASE_URL "https://iot-warehouse-4029a-default-rtdb.firebaseio.com/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// motors
#define BELT1 2
#define BELT2 18
#define G2_1 25
#define G2_2 26

// sensors
#define IR_PIN 32
#define FLAME_PIN 33

#define BUZZER 27
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// lcd
LiquidCrystal_I2C lcd(0x27, 16, 2);

// servo
#define SERVO_PIN 14
Servo gateServo;

// ---------------- STATES & TIMERS ----------------
bool gateMotorRunning = false;
unsigned long servoStartTime = 0;
bool servoRunning = false;
unsigned long gateMotorStartTime = 0;
unsigned long lastDHTRead = 0;

bool beltStoppedByIR = false;
bool beltReversed = false;
unsigned long irStopStartTime = 0;

int lastDoorStatus = 0;
int lastWarehouseDoorStatus = 0;
int lastBeltCmd = 1;

const unsigned long DHT_INTERVAL = 15000;
const unsigned long IR_STOP_DURATION = 30000;

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

  // SERVO
  gateServo.attach(SERVO_PIN);
  gateServo.write(0);

  // dc motors
  pinMode(BELT1, OUTPUT);
  pinMode(BELT2, OUTPUT);
  pinMode(G2_1, OUTPUT);
  pinMode(G2_2, OUTPUT);

  // sensors
  pinMode(IR_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  dht.begin();

  // forward
  conveyorBeltMotorForward();

  // gate1(item gate)
  digitalWrite(G2_1, HIGH);
  digitalWrite(G2_2, LOW);

  lcd.clear();
  lcd.print("System Ready");
}

void loop() {

  // ---------------- DHT ----------------
  if (millis() - lastDHTRead >= DHT_INTERVAL) {
    lastDHTRead = millis();
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (!isnan(humidity) && !isnan(temperature)) {
      Serial.print("Temp: "); Serial.println(temperature);
      Serial.print("Hum: "); Serial.println(humidity);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Temp:"); lcd.print(temperature);
      lcd.setCursor(0, 1);
      lcd.print("Hum:"); lcd.print(humidity);

      Firebase.RTDB.setFloat(&fbdo, "/warehouse/DHT-TEM", temperature);
      Firebase.RTDB.setFloat(&fbdo, "/warehouse/DHT-HUM", humidity);
    }
  }

  // ---------------- FLAME ----------------
  int flameState = digitalRead(FLAME_PIN);
  if (flameState == HIGH) {
    digitalWrite(BUZZER, HIGH);
    Firebase.RTDB.setBool(&fbdo, "/warehouse/Fire_status", true);
    Serial.println("FIRE DETECTED");
    lcd.clear();
    lcd.print("FIRE DETECTED");
  } else {
    digitalWrite(BUZZER, LOW);
    Firebase.RTDB.setBool(&fbdo, "/warehouse/Fire_status", false);
  }

  // ---------------- IR ----------------
  int irState = digitalRead(IR_PIN);
  Firebase.RTDB.setBool(&fbdo, "/warehouse/IR_status", irState == LOW ? false : true);

  if (irState == LOW && !beltStoppedByIR && !beltReversed) {
    conveyorBeltMotorStop();
    irStopStartTime = millis();
    beltStoppedByIR = true;
    Serial.println("IR detected - Belt stopped 30s");
  }

  if (beltStoppedByIR && millis() - irStopStartTime >= IR_STOP_DURATION) {
    conveyorBeltMotorForward();
    beltStoppedByIR = false;
    Serial.println("Belt resumed forward after IR");
  }

  // ---------------- CONTROL BELT FROM Firebase ----------------
  if (!beltStoppedByIR) {
    if (Firebase.RTDB.getInt(&fbdo, "/belt_status")) {
      int beltCmd = fbdo.intData();

      if (beltCmd != lastBeltCmd) {
        lastBeltCmd = beltCmd;

        if (beltCmd == 0) {
          conveyorBeltMotorStop();
          beltReversed = false;
          Serial.println("Belt stopped by Firebase");
        } else if (beltCmd == 2) {
          conveyorBeltMotorReverse();
          beltReversed = true;
          Serial.println("Belt reversed by Firebase");
        } else if (beltCmd == 1) {
          conveyorBeltMotorForward();
          beltReversed = false;
          Serial.println("Belt forward by Firebase");
        }
      }
    }
  }

  // ---------------- GATE MOTOR ----------------
  if (!gateMotorRunning) {
    digitalWrite(G2_1, HIGH);
    digitalWrite(G2_2, LOW);
  }

  if (Firebase.RTDB.getInt(&fbdo, "/door_status")) {
    int doorCmd = fbdo.intData();

    if (doorCmd == 1 && doorCmd != lastDoorStatus && !gateMotorRunning) {
      lastDoorStatus = doorCmd;
      digitalWrite(G2_1, LOW);
      digitalWrite(G2_2, HIGH);
      gateMotorStartTime = millis();
      gateMotorRunning = true;
    }

    if (doorCmd == 0) {
      lastDoorStatus = 0;
    }
  }

  if (gateMotorRunning && millis() - gateMotorStartTime >= 10000) {
    digitalWrite(G2_1, HIGH);
    digitalWrite(G2_2, LOW);
    gateMotorRunning = false;
    Firebase.RTDB.setInt(&fbdo, "/door_status", 0);
  }

  // ---------------- ALARM ----------------
  if (Firebase.RTDB.getInt(&fbdo, "/alarm")) {
    if (fbdo.intData() == 1)
      digitalWrite(BUZZER, HIGH);
    else
      digitalWrite(BUZZER, LOW);
  }

  // ---------------- SERVO ----------------
  if (Firebase.RTDB.getInt(&fbdo, "/warehouse/door_status")) {
    int wDoorCmd = fbdo.intData();

    if (wDoorCmd == 1 && wDoorCmd != lastWarehouseDoorStatus && !servoRunning) {
      lastWarehouseDoorStatus = wDoorCmd;
      gateServo.write(180);
      servoStartTime = millis();
      servoRunning = true;
    }

    if (wDoorCmd == 0) {
      lastWarehouseDoorStatus = 0;
    }
  }

  if (servoRunning && millis() - servoStartTime >= 20000) {
    gateServo.write(0);
    servoRunning = false;
    Firebase.RTDB.setInt(&fbdo, "/warehouse/door_status", 0);
  }
}

// ---------------- BELT FUNCTIONS ----------------
void conveyorBeltMotorForward() {
  digitalWrite(BELT1, HIGH);
  digitalWrite(BELT2, LOW);
}

void conveyorBeltMotorReverse() {
  digitalWrite(BELT1, LOW);
  digitalWrite(BELT2, HIGH);
}

void conveyorBeltMotorStop() {
  digitalWrite(BELT1, LOW);
  digitalWrite(BELT2, LOW);
}