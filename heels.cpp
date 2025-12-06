#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <MPU6050.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define FSR_PIN 4
MPU6050 mpu;

// BLE Variables (Unchanged)
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) { deviceConnected = true; };
  void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
};

// Modified variables for better press detection
int vibpin = 5;
int ledpin = 2, empin = 23;
int pressureThreshold = 100;
unsigned long lastPressTime = 0;
const unsigned long pressWindow = 2000; // 2 seconds for 3 presses
int pressCounter = 0;
bool em = false;
unsigned long emStartTime = 0;
const unsigned long emDuration = 60000; // 1 minute

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");
  Wire.begin(21, 22);
  
  pinMode(FSR_PIN, INPUT);
  pinMode(ledpin, OUTPUT);
  pinMode(empin, OUTPUT);
  pinMode(vibpin, OUTPUT);

  // Original BLE Setup (Unchanged)
  BLEDevice::init("HerSafeStep");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | 
    BLECharacteristic::PROPERTY_WRITE | 
    BLECharacteristic::PROPERTY_NOTIFY | 
    BLECharacteristic::PROPERTY_INDICATE
  );
  BLE2902 *pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);
  pCharacteristic->addDescriptor(pBLE2902);
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Waiting for connection...");
}

void loop() {
  // Improved FSR Press Detection
  int fsrValue = analogRead(FSR_PIN);
  static bool lastState = false;
  
  if (fsrValue >= pressureThreshold && !lastState) {
    if(millis() - lastPressTime > 300) { // Debounce
      pressCounter++;
      lastPressTime = millis();
      digitalWrite(ledpin, HIGH);
      Serial.println("Press detected: " + String(pressCounter));
      
      // Start timing on first press
      if(pressCounter == 1) emStartTime = millis();
    }
    lastState = true;
  }
  else if (fsrValue < pressureThreshold) {
    lastState = false;
    digitalWrite(ledpin, LOW);
  }

  // Handle 3-press sequence
  if(pressCounter >= 3 && !em) {
    // if(millis() - emStartTime <= pressWindow) {
      // if(!em) { // Send SOS only once
        em = true;
        digitalWrite(vibpin, HIGH);
        pCharacteristic->setValue("SOS");
        pCharacteristic->notify();
        Serial.println("EMERGENCY ACTIVATED");
    //   }
    // }
    pressCounter = 0; // Always reset counter
  }

  // Emergency timeout
  if(em && (millis() - emStartTime > emDuration)) {
    em = false;
    digitalWrite(vibpin, LOW);
    digitalWrite(empin, LOW);
    Serial.println("Emergency ended");
  }

  // Original BLE Connection Handling (Unchanged)
  if(!deviceConnected && oldDeviceConnected) {
    pServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
    Serial.println("Advertising started");
  }
  if(deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    Serial.println("Connected");
  }

  delay(10);
}
