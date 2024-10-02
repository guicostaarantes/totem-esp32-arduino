#include "BLEDevice.h"

#define SERVICE_UUID_LEFT "9891ffda-e4a9-42ef-81d0-ddb36d18af3a"
#define CHARACTERISTIC_UUID_LEFT "d374ec4f-0564-45f0-877a-9ef5ebed83ab"
#define SERVICE_UUID_RIGHT "323d5e73-543e-40e4-a573-c03d694ad2c8"
#define CHARACTERISTIC_UUID_RIGHT "fc489b69-99aa-437a-ace8-0d749e16ebfe"

BLEUUID serviceUuidLeft(SERVICE_UUID_LEFT);
BLEUUID charUuidLeft(CHARACTERISTIC_UUID_LEFT);
BLEUUID serviceUuidRight(SERVICE_UUID_RIGHT);
BLEUUID charUuidRight(CHARACTERISTIC_UUID_RIGHT);

BLEScan *pBLEScan;

bool scanning = true;

bool foundLeft = false;
bool connectedLeft = false;
BLEAdvertisedDevice *deviceLeft;

bool foundRight = false;
bool connectedRight = false;
BLEAdvertisedDevice *deviceRight;

void notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify) {
  int val = *reinterpret_cast<int*>(pData);
  if (pBLERemoteCharacteristic->getUUID().toString() == CHARACTERISTIC_UUID_LEFT) {
    Serial.print("Left says ");
    Serial.println(val);
    updateLeftState(val);
  }
  if (pBLERemoteCharacteristic->getUUID().toString() == CHARACTERISTIC_UUID_RIGHT) {
    Serial.print("Right says ");
    Serial.println(val);
  }
}

class LeftClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient *pclient) {}

  void onDisconnect(BLEClient *pclient) {
    connectedLeft = false;
    Serial.println("Left - Disconnected");
  }
};

bool connectToLeft() {
  Serial.print("Connecting to Left on device with MAC address ");
  Serial.println(deviceLeft->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  Serial.println("Left - Created client");

  pClient->setClientCallbacks(new LeftClientCallback());

  pClient->connect(deviceLeft);
  Serial.println("Left - Connected to server");

  BLERemoteService *pRemoteService = pClient->getService(serviceUuidLeft);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUuidLeft.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println("Left - Found our service");

  BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getCharacteristic(charUuidLeft);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUuidLeft.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println("Left - Found our characteristic");

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  connectedLeft = true;
  return true;
}

class RightClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient *pclient) {}

  void onDisconnect(BLEClient *pclient) {
    connectedRight = false;
    Serial.println("Right - Disconnected");
  }
};

bool connectToRight() {
  Serial.print("Connecting to Right on device with MAC address ");
  Serial.println(deviceRight->getAddress().toString().c_str());

  BLEClient *pClient = BLEDevice::createClient();
  Serial.println("Right - Created client");

  pClient->setClientCallbacks(new RightClientCallback());

  pClient->connect(deviceRight);
  Serial.println("Right - Connected to server");

  BLERemoteService *pRemoteService = pClient->getService(serviceUuidRight);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUuidRight.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println("Right - Found our service");

  BLERemoteCharacteristic *pRemoteCharacteristic = pRemoteService->getCharacteristic(charUuidRight);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUuidRight.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println("Right - Found our characteristic");

  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }

  connectedRight = true;
  Serial.println("Right - Finished connecting");
  return true;
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
    if (advertisedDevice.haveServiceUUID()) {
      if (!connectedLeft && advertisedDevice.isAdvertisingService(serviceUuidLeft)) {
        deviceLeft = new BLEAdvertisedDevice(advertisedDevice);
        foundLeft = true;
      }
      if (!connectedRight && advertisedDevice.isAdvertisingService(serviceUuidRight)) {
        deviceRight = new BLEAdvertisedDevice(advertisedDevice);
        foundRight = true;
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("Totem Dongle");

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);

  setupKeyboard();
}

void loop() {
  if (!connectedLeft || !connectedRight) {
    Serial.println("Scanning for keyboards");
    foundLeft = false;
    foundRight = false;
    pBLEScan->start(5, true);
    if (foundLeft) {
      if (connectToLeft()) {
        Serial.println("Left is connected");
      } else {
        Serial.println("Left was found but connection failed");
      }
    } else {
      if (!connectedLeft) {
        Serial.println("Could not find Left, trying again");
      }
    }
    if (foundRight) {
      if (connectToRight()) {
        Serial.println("Right is connected");
      } else {
        Serial.println("Right was found but connection failed");
      }
    } else {
      if (!connectedRight) {
        Serial.println("Could not find Right, trying again");
      }
    }
  } else {
    delay(2000);
  }
}
