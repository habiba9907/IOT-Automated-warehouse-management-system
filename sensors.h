#pragma once
#include <Arduino.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include "firebase_config.h"

#define FLAME_PIN 33
#define BUZZER 27
#define DHTPIN 15
#define DHTTYPE DHT11

extern DHT dht;
extern LiquidCrystal_I2C lcd;
extern bool fireActive;
extern bool fireClearedShown;
extern unsigned long fireClearedTime;
extern unsigned long lastDHTRead;

const unsigned long FIRE_CLEARED_DURATION = 3000;
const unsigned long DHT_INTERVAL = 15000;

void handleDHT() {
  if (millis() - lastDHTRead >= DHT_INTERVAL && !fireActive && !fireClearedShown) {
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
    } else {
      Serial.println("DHT read failed");
    }
  }
}

void handleFlame() {
  int flameState = digitalRead(FLAME_PIN);

  if (flameState == HIGH) {
    digitalWrite(BUZZER, HIGH);
    Firebase.RTDB.setBool(&fbdo, "/warehouse/Fire_status", true);

    if (!fireActive) {
      fireActive = true;
      fireClearedShown = false;
      lcd.clear();
      lcd.print("FIRE DETECTED");
      Serial.println("FIRE DETECTED");
    }

  } else {
    digitalWrite(BUZZER, LOW);
    Firebase.RTDB.setBool(&fbdo, "/warehouse/Fire_status", false);

    if (fireActive) {
      fireActive = false;
      fireClearedShown = true;
      fireClearedTime = millis();
      lcd.clear();
      lcd.print("Fire Cleared");
      Serial.println("Fire Cleared");
    }
  }

  if (fireClearedShown && millis() - fireClearedTime >= FIRE_CLEARED_DURATION) {
    fireClearedShown = false;
    lastDHTRead = millis() - DHT_INTERVAL;
  }
}
void handleAlarm() {
  if (Firebase.RTDB.getInt(&fbdo, "/alarm")) {
    if (fbdo.intData() == 1) {
      Firebase.RTDB.setBool(&fbdo, "/warehouse/buzzer", true);
      Firebase.RTDB.setBool(&fbdo, "/warehouse/buzzer_status", true);
      Serial.println("Alarm ON");
    } else {
      Firebase.RTDB.setBool(&fbdo, "/warehouse/buzzer", false);
      Firebase.RTDB.setBool(&fbdo, "/warehouse/buzzer_status", false);
      Serial.println("Alarm OFF");
    }
  }
}
