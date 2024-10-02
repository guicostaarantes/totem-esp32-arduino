#include <Arduino.h>
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"

// HID keyboard based on https://gist.github.com/manuelbl/66f059effc8a7be148adb1f104666467

bool isBleConnected = false;

// Message (report) sent when a key is pressed or released
struct InputReport {
  uint8_t modifiers;	     // bitmask: CTRL = 1, SHIFT = 2, ALT = 4
  uint8_t reserved;        // must be 0
  uint8_t pressedKeys[6];  // up to six concurrenlty pressed keys
};

// Message (report) received when an LED's state changed
struct OutputReport {
  uint8_t leds;            // bitmask: num lock = 1, caps lock = 2, scroll lock = 4, compose = 8, kana = 16
};

// The report map describes the HID device (a keyboard in this case) and
// the messages (reports in HID terms) sent and received.
static const uint8_t REPORT_MAP[] = {
  USAGE_PAGE(1),      0x01,       // Generic Desktop Controls
  USAGE(1),           0x06,       // Keyboard
  COLLECTION(1),      0x01,       // Application
  REPORT_ID(1),       0x01,       //   Report ID (1)
  USAGE_PAGE(1),      0x07,       //   Keyboard/Keypad
  USAGE_MINIMUM(1),   0xE0,       //   Keyboard Left Control
  USAGE_MAXIMUM(1),   0xE7,       //   Keyboard Right Control
  LOGICAL_MINIMUM(1), 0x00,       //   Each bit is either 0 or 1
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_COUNT(1),    0x08,       //   8 bits for the modifier keys
  REPORT_SIZE(1),     0x01,       
  HIDINPUT(1),        0x02,       //   Data, Var, Abs
  REPORT_COUNT(1),    0x01,       //   1 byte (unused)
  REPORT_SIZE(1),     0x08,
  HIDINPUT(1),        0x01,       //   Const, Array, Abs
  REPORT_COUNT(1),    0x06,       //   6 bytes (for up to 6 concurrently pressed keys)
  REPORT_SIZE(1),     0x08,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
  USAGE_MINIMUM(1),   0x00,
  USAGE_MAXIMUM(1),   0x65,
  HIDINPUT(1),        0x00,       //   Data, Array, Abs
  REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
  REPORT_SIZE(1),     0x01,
  USAGE_PAGE(1),      0x08,       //   LEDs
  USAGE_MINIMUM(1),   0x01,       //   Num Lock
  USAGE_MAXIMUM(1),   0x05,       //   Kana
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  HIDOUTPUT(1),       0x02,       //   Data, Var, Abs
  REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
  REPORT_SIZE(1),     0x03,
  HIDOUTPUT(1),       0x01,       //   Const, Array, Abs
  END_COLLECTION(0)               // End application collection
};

const InputReport NO_KEY_PRESSED = {
    .modifiers = 0,
    .reserved = 0,
    .pressedKeys = { 0, 0, 0, 0, 0, 0 }
  };

BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;

class BleKeyboardCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) {
    isBleConnected = true;

    // Allow notifications for characteristics
    BLE2902* cccDesc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    cccDesc->setNotifications(true);

    Serial.println("Client has connected");
  }

  void onDisconnect(BLEServer* server) {
    isBleConnected = false;

        // Disallow notifications for characteristics
    BLE2902* cccDesc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    cccDesc->setNotifications(false);

    Serial.println("Client has disconnected");
  }
};


/*
 * Called when the client (computer, smart phone) wants to turn on or off
 * the LEDs in the keyboard.
 * 
 * bit 0 - NUM LOCK
 * bit 1 - CAPS LOCK
 * bit 2 - SCROLL LOCK
 */
class OutputCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) {
    OutputReport* report = (OutputReport*) characteristic->getData();
    Serial.print("LED state: ");
    Serial.print((int) report->leds);
    Serial.println();
  }
};

void setupKeyboard() {
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new BleKeyboardCallbacks());

  // create an HID device
  hid = new BLEHIDDevice(server);
  input = hid->inputReport(1); // report ID
  output = hid->outputReport(1); // report ID
  output->setCallbacks(new OutputCallbacks());

  // set manufacturer name
  hid->manufacturer()->setValue("Totem");
  // set USB vendor and product ID
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  // information about HID device: device is not localized, device can be connected
  hid->hidInfo(0x00, 0x02);

  // Security: device requires bonding
  BLESecurity* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_BOND);

  // set report map
  hid->reportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
  hid->startServices();

  // set battery level to 100%
  // hid->setBatteryLevel(100);

  // advertise the services
  BLEAdvertising* advertising = server->getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(hid->hidService()->getUUID());
  advertising->addServiceUUID(hid->deviceInfo()->getUUID());
  // advertising->addServiceUUID(hid->batteryService()->getUUID());
  advertising->start();

  Serial.println("Start advertising");
}

// End of HID keyboard based on https://gist.github.com/manuelbl/66f059effc8a7be148adb1f104666467

// Layers:
uint8_t layer = 0;
// 0: Qwerty
// 1: Colemak DH with Homerow mods (send key on release)

int last_state_left = 0;
int last_state_right = 0;
uint8_t modifiers = 0;
uint8_t pressedKeys[6] = { 0, 0, 0, 0, 0, 0 };

void sendReport() {
  InputReport report;
  report.modifiers = modifiers;
  report.reserved = 0;
  memcpy(report.pressedKeys, pressedKeys, sizeof(pressedKeys));
  input->setValue((uint8_t*)&report, sizeof(report));
  input->notify();
}

void pressModifier(uint8_t mod) {
  modifiers |= mod;
  sendReport();
}

void releaseModifier(uint8_t mod) {
  modifiers &= ~mod;
  sendReport();
}

void pressKey(uint8_t key) {
  for (uint8_t i = 0; i < 6; i++) {
    if (pressedKeys[i] == 0) {
      // found first empty slot
      pressedKeys[i] = key;
      break;
    }
  }  
  sendReport();
}

void releaseKey(uint8_t key) {
  for (uint8_t i = 0; i < 6; i++) {
    if (pressedKeys[i] == key) {
      // found slot with released key
      for (uint8_t j = i + 1; j < 6; j++) {
        pressedKeys[j - 1] = pressedKeys[j];
      }
      pressedKeys[5] = 0;
      break;
    }
  }  
  sendReport();
}

void pressAndReleaseKey(uint8_t key) {
  InputReport report = {
    .modifiers = modifiers,
    .reserved = 0,
    .pressedKeys = { key, 0, 0, 0, 0, 0 }
  };
  input->setValue((uint8_t*)&report, sizeof(report));
  input->notify();
  delay(5);
  input->setValue((uint8_t*)&NO_KEY_PRESSED, sizeof(NO_KEY_PRESSED));
  input->notify();
}

void updateLeftState(int new_state) {
  if (layer == 0) {
    if (~last_state_left & 1 << 0 && new_state & 1 << 0) { // key 0 is pressed
      pressKey(0x14); // q
    }
  
    if (last_state_left & 1 << 0 && ~new_state & 1 << 0) { // key 0 is released
      releaseKey(0x14); // q
    }
  
    if (~last_state_left & 1 << 1 && new_state & 1 << 1) { // key 1 is pressed
      pressKey(0x04); // a
    }
  
    if (last_state_left & 1 << 1 && ~new_state & 1 << 1) { // key 1 is released
      releaseKey(0x04); // a
    }
  
    if (~last_state_left & 1 << 2 && new_state & 1 << 2) { // key 2 is pressed
      pressKey(0x1d); // z
    }
  
    if (last_state_left & 1 << 2 && ~new_state & 1 << 2) { // key 2 is released
      releaseKey(0x1d); // z
    }
  
    if (~last_state_left & 1 << 3 && new_state & 1 << 3) { // key 3 is pressed
    }
  
    if (last_state_left & 1 << 3 && ~new_state & 1 << 3) { // key 3 is released
      layer = 1;
    }
  
    if (~last_state_left & 1 << 4 && new_state & 1 << 4) { // key 4 is pressed
      pressKey(0x1a); // w
    }
  
    if (last_state_left & 1 << 4 && ~new_state & 1 << 4) { // key 4 is released
      releaseKey(0x1a); // w
    }
  
    if (~last_state_left & 1 << 5 && new_state & 1 << 5) { // key 5 is pressed
      pressKey(0x16); // s
    }
  
    if (last_state_left & 1 << 5 && ~new_state & 1 << 5) { // key 5 is released
      releaseKey(0x16); // s
    }
  
    if (~last_state_left & 1 << 6 && new_state & 1 << 6) { // key 6 is pressed
      pressKey(0x1b); // x
    }
  
    if (last_state_left & 1 << 6 && ~new_state & 1 << 6) { // key 6 is released
      releaseKey(0x1b); // x
    }
  
    if (~last_state_left & 1 << 7 && new_state & 1 << 7) { // key 7 is pressed
      pressKey(0x08); // e
    }
  
    if (last_state_left & 1 << 7 && ~new_state & 1 << 7) { // key 7 is released
      releaseKey(0x08); // e
    }
  
    if (~last_state_left & 1 << 8 && new_state & 1 << 8) { // key 8 is pressed
      pressKey(0x07); // d
    }

    if (last_state_left & 1 << 8 && ~new_state & 1 << 8) { // key 8 is released
      releaseKey(0x07); // d
    }
  
    if (~last_state_left & 1 << 9 && new_state & 1 << 9) { // key 9 is pressed
      pressKey(0x06); // c
    }
  
    if (last_state_left & 1 << 9 && ~new_state & 1 << 9) { // key 9 is released
      releaseKey(0x06); // c
    }
  
    if (~last_state_left & 1 << 10 && new_state & 1 << 10) { // key 10 is pressed
      pressKey(0x29); // escape
    }
  
    if (last_state_left & 1 << 10 && ~new_state & 1 << 10) { // key 10 is released
      releaseKey(0x29); // escape
    }
  
    if (~last_state_left & 1 << 11 && new_state & 1 << 11) { // key 11 is pressed
      pressKey(0x15); // r
    }
  
    if (last_state_left & 1 << 11 && ~new_state & 1 << 11) { // key 11 is released
      releaseKey(0x15); // r
    }
  
    if (~last_state_left & 1 << 12 && new_state & 1 << 12) { // key 12 is pressed
      pressKey(0x09); // f
    }
  
    if (last_state_left & 1 << 12 && ~new_state & 1 << 12) { // key 12 is released
      releaseKey(0x09); // f
    }
  
    if (~last_state_left & 1 << 13 && new_state & 1 << 13) { // key 13 is pressed
      pressKey(0x19); // v
    }
  
    if (last_state_left & 1 << 13 && ~new_state & 1 << 13) { // key 13 is released
      releaseKey(0x19); // v
    }
  
    if (~last_state_left & 1 << 14 && new_state & 1 << 14) { // key 14 is pressed
      pressKey(0x2a); // backspace
    }
  
    if (last_state_left & 1 << 14 && ~new_state & 1 << 14) { // key 14 is released
      releaseKey(0x2a); // backspace
    }
  
    if (~last_state_left & 1 << 15 && new_state & 1 << 15) { // key 15 is pressed
      pressKey(0x17); // t
    }
  
    if (last_state_left & 1 << 15 && ~new_state & 1 << 15) { // key 15 is released
      releaseKey(0x17); // t
    }
  
    if (~last_state_left & 1 << 16 && new_state & 1 << 16) { // key 16 is pressed
      pressKey(0x0a); // g
    }
  
    if (last_state_left & 1 << 16 && ~new_state & 1 << 16) { // key 16 is released
      releaseKey(0x0a); // g
    }
  
    if (~last_state_left & 1 << 17 && new_state & 1 << 17) { // key 17 is pressed
      pressKey(0x05); // b
    }
  
    if (last_state_left & 1 << 17 && ~new_state & 1 << 17) { // key 17 is released
      releaseKey(0x05); // b
    }
  
    if (~last_state_left & 1 << 18 && new_state & 1 << 18) { // key 18 is pressed
      pressKey(0x2c); // space
    }
  
    if (last_state_left & 1 << 18 && ~new_state & 1 << 18) { // key 18 is released
      releaseKey(0x2c); // space
    }
  } else if (layer == 1) {
    if (~last_state_left & 1 << 0 && new_state & 1 << 0) { // key 0 is pressed
    }
  
    if (last_state_left & 1 << 0 && ~new_state & 1 << 0) { // key 0 is released
        pressAndReleaseKey(0x14); // q
    }
  
    if (~last_state_left & 1 << 1 && new_state & 1 << 1) { // key 1 is pressed
    }
  
    if (last_state_left & 1 << 1 && ~new_state & 1 << 1) { // key 1 is released
        pressAndReleaseKey(0x04); // a
    }
  
    if (~last_state_left & 1 << 2 && new_state & 1 << 2) { // key 2 is pressed
    }
  
    if (last_state_left & 1 << 2 && ~new_state & 1 << 2) { // key 2 is released
        pressAndReleaseKey(0x1d); // z
    }
  
    if (~last_state_left & 1 << 3 && new_state & 1 << 3) { // key 3 is pressed
    }
  
    if (last_state_left & 1 << 3 && ~new_state & 1 << 3) { // key 3 is released
      layer = 0;
    }
  
    if (~last_state_left & 1 << 4 && new_state & 1 << 4) { // key 4 is pressed
    }
  
    if (last_state_left & 1 << 4 && ~new_state & 1 << 4) { // key 4 is released
        pressAndReleaseKey(0x1a); // w
    }
  
    if (~last_state_left & 1 << 5 && new_state & 1 << 5) { // key 5 is pressed
    }
  
    if (last_state_left & 1 << 5 && ~new_state & 1 << 5) { // key 5 is released
        pressAndReleaseKey(0x15); // r
    }
  
    if (~last_state_left & 1 << 6 && new_state & 1 << 6) { // key 6 is pressed
    }
  
    if (last_state_left & 1 << 6 && ~new_state & 1 << 6) { // key 6 is released
        pressAndReleaseKey(0x1b); // x
    }
  
    if (~last_state_left & 1 << 7 && new_state & 1 << 7) { // key 7 is pressed
    }
  
    if (last_state_left & 1 << 7 && ~new_state & 1 << 7) { // key 7 is released
        pressAndReleaseKey(0x09); // f
    }
  
    if (~last_state_left & 1 << 8 && new_state & 1 << 8) { // key 8 is pressed
    }
  
    if (last_state_left & 1 << 8 && ~new_state & 1 << 8) { // key 8 is released
        pressAndReleaseKey(0x16); // s
    }
  
    if (~last_state_left & 1 << 9 && new_state & 1 << 9) { // key 9 is pressed
    }
  
    if (last_state_left & 1 << 9 && ~new_state & 1 << 9) { // key 9 is released
        pressAndReleaseKey(0x06); // c
    }
  
    if (~last_state_left & 1 << 10 && new_state & 1 << 10) { // key 10 is pressed
    }
  
    if (last_state_left & 1 << 10 && ~new_state & 1 << 10) { // key 10 is released
      pressAndReleaseKey(0x2a); // backspace
    }
  
    if (~last_state_left & 1 << 11 && new_state & 1 << 11) { // key 11 is pressed
    }
  
    if (last_state_left & 1 << 11 && ~new_state & 1 << 11) { // key 11 is released
        pressAndReleaseKey(0x13); // p
    }
  
    if (~last_state_left & 1 << 12 && new_state & 1 << 12) { // key 12 is pressed
    }
  
    if (last_state_left & 1 << 12 && ~new_state & 1 << 12) { // key 12 is released
        pressAndReleaseKey(0x17); // t
    }
  
    if (~last_state_left & 1 << 13 && new_state & 1 << 13) { // key 13 is pressed
    }
  
    if (last_state_left & 1 << 13 && ~new_state & 1 << 13) { // key 13 is released
        pressAndReleaseKey(0x07); // d
    }
  
    if (~last_state_left & 1 << 14 && new_state & 1 << 14) { // key 14 is pressed
    }
  
    if (last_state_left & 1 << 14 && ~new_state & 1 << 14) { // key 14 is released
    }
  
    if (~last_state_left & 1 << 15 && new_state & 1 << 15) { // key 15 is pressed
    }
  
    if (last_state_left & 1 << 15 && ~new_state & 1 << 15) { // key 15 is released
        pressAndReleaseKey(0x05); // b
    }
  
    if (~last_state_left & 1 << 16 && new_state & 1 << 16) { // key 16 is pressed
    }
  
    if (last_state_left & 1 << 16 && ~new_state & 1 << 16) { // key 16 is released
        pressAndReleaseKey(0x0a); // g
    }
  
    if (~last_state_left & 1 << 17 && new_state & 1 << 17) { // key 17 is pressed
    }
  
    if (last_state_left & 1 << 17 && ~new_state & 1 << 17) { // key 17 is released
        pressAndReleaseKey(0x19); // v
    }
  
    if (~last_state_left & 1 << 18 && new_state & 1 << 18) { // key 18 is pressed
    }
  
    if (last_state_left & 1 << 18 && ~new_state & 1 << 18) { // key 18 is released
    }
  }

  last_state_left = new_state;
}
