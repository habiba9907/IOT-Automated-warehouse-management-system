# IoT Automated Warehouse Management System

Graduation project: an ESP32-based system that monitors and controls a small automated warehouse, integrated with a mobile app through Firebase Realtime Database.

## Features
- App-controlled gate: commands sent from the app travel through Firebase to the ESP32, which operates the gate servo and closes it automatically after 20 seconds.
- Conveyor belt control: start/stop from the app, plus an IR sensor that stops the belt when an item arrives.
- Fire detection: a flame sensor triggers a buzzer, an LCD message, and a status update in Firebase.
- Environment monitoring: DHT11 temperature and humidity readings shown on an LCD and pushed to Firebase.
- Reliability: non-blocking timing with millis() and automatic WiFi reconnection.

## Architecture
Mobile App <-> Firebase Realtime Database <-> ESP32 <-> Sensors & Actuators

## Hardware
ESP32, servo motor, DC motors with L298N driver, IR sensor, flame sensor, DHT11, buzzer, 16x2 I2C LCD.

## Libraries
Firebase_ESP_Client, ESP32Servo, DHT sensor library, LiquidCrystal_I2C

## Project structure
- firmware/: ESP32 code, split into sensors.h, belt.h, gates.h, and firebase_config.h
- old/: earlier single-file version

## Setup
1. Open firmware/firmware.ino in Arduino IDE.
2. Edit firebase_config.h and the WiFi credentials with your own values.
3. Select an ESP32 board and upload.

## My role
Hardware setup, embedded code (C++), Firebase connectivity, and integration with the app.
