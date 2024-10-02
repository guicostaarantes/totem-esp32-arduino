#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define COL0 D4
#define COL1 D5
#define COL2 D10
#define COL3 D9
#define COL4 D8

#define ROW0 D0
#define ROW1 D1
#define ROW2 D2
#define ROW3 D3

#define DELAY_MTX_MS 8

#define SERVICE_UUID "323d5e73-543e-40e4-a573-c03d694ad2c8"
#define CHARACTERISTIC_UUID "fc489b69-99aa-437a-ace8-0d749e16ebfe"

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;

bool isConnected = false;
bool isAdvertising = false;

int last_state = 0;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    isConnected = true;
    isAdvertising = false;
  };

  void onDisconnect(BLEServer *pServer) {
    isConnected = false;
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(COL0, OUTPUT);
  pinMode(COL1, OUTPUT);
  pinMode(COL2, OUTPUT);
  pinMode(COL3, OUTPUT);
  pinMode(COL4, OUTPUT);

  pinMode(ROW0, INPUT);
  pinMode(ROW1, INPUT);
  pinMode(ROW2, INPUT);
  pinMode(ROW3, INPUT);

  BLEDevice::init("");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
}

void loop() {
  if (isConnected) {
    int state = 0;

    digitalWrite(COL0, HIGH);
    state |= digitalRead(ROW0) << 0;
    state |= digitalRead(ROW1) << 1;
    state |= digitalRead(ROW2) << 2;
    state |= digitalRead(ROW3) << 3;
    digitalWrite(COL0, LOW);
    delay(DELAY_MTX_MS);

    digitalWrite(COL1, HIGH);
    state |= digitalRead(ROW0) << 4;
    state |= digitalRead(ROW1) << 5;
    state |= digitalRead(ROW2) << 6;
    digitalWrite(COL1, LOW);
    delay(DELAY_MTX_MS);

    digitalWrite(COL2, HIGH);
    state |= digitalRead(ROW0) << 7;
    state |= digitalRead(ROW1) << 8;
    state |= digitalRead(ROW2) << 9;
    state |= digitalRead(ROW3) << 10;
    digitalWrite(COL2, LOW);
    delay(DELAY_MTX_MS);

    digitalWrite(COL3, HIGH);
    state |= digitalRead(ROW0) << 11;
    state |= digitalRead(ROW1) << 12;
    state |= digitalRead(ROW2) << 13;
    state |= digitalRead(ROW3) << 14;
    digitalWrite(COL3, LOW);
    delay(DELAY_MTX_MS);

    digitalWrite(COL4, HIGH);
    state |= digitalRead(ROW0) << 15;
    state |= digitalRead(ROW1) << 16;
    state |= digitalRead(ROW2) << 17;
    state |= digitalRead(ROW3) << 18;
    digitalWrite(COL4, LOW);
    delay(DELAY_MTX_MS);

    if (state != last_state) {
      pCharacteristic->setValue(state);
      pCharacteristic->notify();
      last_state = state;
      Serial.println(state);
    }
  } else if (!isAdvertising) {
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Start advertising");
    isAdvertising = true;
  }
}
