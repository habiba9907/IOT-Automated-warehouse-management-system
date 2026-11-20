#pragma once
#include <Arduino.h>
#include <ESP32Servo.h>
#include "firebase_config.h"

#define SERVO_PIN 14
#define G2_1 4
#define G2_2 5

extern Servo gateServo;
extern bool gateMotorRunning;
extern unsigned long gateMotorStartTime;
extern bool servoRunning;
extern unsigned long servoStartTime;
extern int lastDoorStatus;
extern int lastWarehouseDoorStatus;

void initGateMotor() {
  digitalWrite(G2_1, LOW);
  digitalWrite(G2_2, LOW);
}

void handleGateMotor() {
  if (gateMotorRunning && millis() - gateMotorStartTime >= 5000) {
    digitalWrite(G2_1, LOW);
    digitalWrite(G2_2, LOW);
    gateMotorRunning = false;
    Serial.println("Gate motor stopped");
  }
}

void startGateMotorOpen() {
  if (!gateMotorRunning) {
    digitalWrite(G2_1, HIGH);
    digitalWrite(G2_2, LOW);
    gateMotorStartTime = millis();
    gateMotorRunning = true;
    Serial.println("Gate motor opening");
  }
}

void startGateMotorClose() {
  if (!gateMotorRunning) {
    digitalWrite(G2_1, LOW);
    digitalWrite(G2_2, HIGH);
    gateMotorStartTime = millis();
    gateMotorRunning = true;
    Serial.println("Gate motor closing");
  }
}

void handleServo() {
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
  } else {
    Serial.print("Servo Firebase error: ");
    Serial.println(fbdo.errorReason());
  }

  if (servoRunning && millis() - servoStartTime >= 20000) {
    gateServo.write(0);
    servoRunning = false;
    Firebase.RTDB.setBool(&fbdo, "/warehouse/door_status", false);
  }
}
