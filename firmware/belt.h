#pragma once
#include <Arduino.h>
#include "firebase_config.h"

#define BELT_IN1 26
#define BELT_IN2 25
#define BELT_ENA 25
#define IR_PIN 13

extern bool beltStoppedByIR;
extern bool beltReversed;  
extern unsigned long irStopStartTime;
extern unsigned long lastIRFirebaseUpdate;

const unsigned long IR_STOP_DURATION = 30000;
const unsigned long IR_FIREBASE_INTERVAL = 2000;

void conveyorBeltMotorForward() {
  digitalWrite(BELT_IN1, HIGH);
  digitalWrite(BELT_IN2, LOW);
}

void conveyorBeltMotorReverse() {
  digitalWrite(BELT_ENA, HIGH);
  digitalWrite(BELT_IN1, LOW);
  digitalWrite(BELT_IN2, HIGH);
}

void conveyorBeltMotorStop() {
  digitalWrite(BELT_ENA, LOW);
  digitalWrite(BELT_IN1, LOW);
  digitalWrite(BELT_IN2, LOW);
}

void handleIR() {
  int irState = digitalRead(IR_PIN);
  Serial.print("IR: "); Serial.println(irState);

  if (irState == LOW) {
    conveyorBeltMotorStop();
    Serial.println("IR detected - Belt stopped");
  } else {
    if (beltStoppedByIR) {
      conveyorBeltMotorForward();
      beltStoppedByIR = false;
      Serial.println("Belt resumed forward");
    }
  }

  if (irState == LOW && !beltStoppedByIR) {
    beltStoppedByIR = true;
  }

  if (millis() - lastIRFirebaseUpdate >= IR_FIREBASE_INTERVAL) {
    lastIRFirebaseUpdate = millis();
    Firebase.RTDB.setBool(&fbdo, "/warehouse/IR_status", irState == LOW ? false : true);
  }
}

void handleBeltFirebase(int &lastBeltCmd) {
  if (!beltStoppedByIR) {
   if (Firebase.RTDB.getBool(&fbdo, "/belt_status")) {
      int beltCmd = fbdo.intData();
      if (beltCmd != lastBeltCmd) {
        lastBeltCmd = beltCmd;
        if (beltCmd == 0) {
          conveyorBeltMotorStop();
          beltReversed = false;
          Serial.println("Belt stopped by Firebase");
        } else if (beltCmd == 1) {
          conveyorBeltMotorForward();
          beltReversed = false;
          Serial.println("Belt forward by Firebase");
        } else if (beltCmd == 2) {
          conveyorBeltMotorReverse();
          beltReversed = true;
          Serial.println("Belt reversed by Firebase");
        }
      }
    } else {
      Serial.print("Belt Firebase error: ");
      Serial.println(fbdo.errorReason());
    }
  }
}
