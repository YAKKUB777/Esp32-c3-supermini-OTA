#include "USB.h"
#include "USBHID.h"

// ====================== Піни ======================
#define JOY1_X    1
#define JOY1_Y    2
#define JOY2_X    3
#define JOY2_Y    4

#define JOY1_SW   5
#define JOY2_SW   6
#define BTN_A     7
#define BTN_B     8
#define BTN_X     9
#define BTN_Y     10
#define LED_PIN   20

// ====================== Змінні калібрування ======================
int joy1X_min = 4095, joy1X_max = 0;
int joy1Y_min = 4095, joy1Y_max = 0;
int joy2X_min = 4095, joy2X_max = 0;
int joy2Y_min = 4095, joy2Y_max = 0;
int joy1X_center, joy1Y_center;
int joy2X_center, joy2Y_center;

bool calibrated = false;

// ====================== USB HID ======================
USBHID HID;
static const uint8_t reportDescriptor[] = {
  0x05, 0x01, // Usage Page (Generic Desktop)
  0x09, 0x05, // Usage (Game Pad)
  0xA1, 0x01, // Collection (Application)
  0x85, 0x01, //   Report ID (1)
  // Лівий джойстик (X, Y)
  0x09, 0x30, //   Usage (X)
  0x09, 0x31, //   Usage (Y)
  0x15, 0x81, //   Logical Minimum (-127)
  0x25, 0x7F, //   Logical Maximum (127)
  0x75, 0x08, //   Report Size (8)
  0x95, 0x02, //   Report Count (2)
  0x81, 0x02, //   Input (Data, Var, Abs)
  // Правий джойстик (Z, Rz)
  0x09, 0x32, //   Usage (Z)
  0x09, 0x35, //   Usage (Rz)
  0x15, 0x81, //   Logical Minimum (-127)
  0x25, 0x7F, //   Logical Maximum (127)
  0x75, 0x08, //   Report Size (8)
  0x95, 0x02, //   Report Count (2)
  0x81, 0x02, //   Input (Data, Var, Abs)
  // Кнопки (8 штук)
  0x05, 0x09, //   Usage Page (Button)
  0x19, 0x01, //   Usage Minimum (1)
  0x29, 0x08, //   Usage Maximum (8)
  0x15, 0x00, //   Logical Minimum (0)
  0x25, 0x01, //   Logical Maximum (1)
  0x75, 0x01, //   Report Size (1)
  0x95, 0x08, //   Report Count (8)
  0x81, 0x02, //   Input (Data, Var, Abs)
  0xC0        // End Collection
};

void setup() {
  Serial.begin(115200);
  
  // Кнопки
  pinMode(JOY1_SW, INPUT_PULLUP);
  pinMode(JOY2_SW, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_X, INPUT_PULLUP);
  pinMode(BTN_Y, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  
  analogReadResolution(12);
  
  // Калібрування
  calibrate();
  
  // USB HID
  HID.begin();
  HID.addDevice(reportDescriptor, sizeof(reportDescriptor));
  USB.begin();
  
  Serial.println("USB Gamepad ready");
}

void loop() {
  if (!calibrated) return;
  
  // Читання аналогових значень
  int x1 = analogRead(JOY1_X);
  int y1 = analogRead(JOY1_Y);
  int x2 = analogRead(JOY2_X);
  int y2 = analogRead(JOY2_Y);
  
  int deadZone = (joy1X_max - joy1X_min) / 20;
  
  int jx1 = mapToAxis(x1, joy1X_min, joy1X_max, joy1X_center, deadZone);
  int jy1 = mapToAxis(y1, joy1Y_min, joy1Y_max, joy1Y_center, deadZone);
  int jx2 = mapToAxis(x2, joy2X_min, joy2X_max, joy2X_center, deadZone);
  int jy2 = mapToAxis(y2, joy2Y_min, joy2Y_max, joy2Y_center, deadZone);
  
  // Кнопки
  uint8_t buttons = 0;
  if (digitalRead(JOY1_SW) == LOW) buttons |= 0x01;
  if (digitalRead(JOY2_SW) == LOW) buttons |= 0x02;
  if (digitalRead(BTN_A) == LOW)   buttons |= 0x04;
  if (digitalRead(BTN_B) == LOW)   buttons |= 0x08;
  if (digitalRead(BTN_X) == LOW)   buttons |= 0x10;
  if (digitalRead(BTN_Y) == LOW)   buttons |= 0x20;
  
  // Формуємо звіт
  uint8_t report[8];
  report[0] = (uint8_t)(jx1 + 128);
  report[1] = (uint8_t)(jy1 + 128);
  report[2] = (uint8_t)(jx2 + 128);
  report[3] = (uint8_t)(jy2 + 128);
  report[4] = buttons;
  report[5] = 0;
  report[6] = 0;
  report[7] = 0;
  
  // Надсилання через USB HID
  HID.sendReport(1, report, 8);
  
  delay(2);
}

void calibrate() {
  Serial.println("Calibrating...");
  digitalWrite(LED_PIN, LOW);
  
  for (int i = 0; i < 500; i++) {
    int x1 = analogRead(JOY1_X);
    int y1 = analogRead(JOY1_Y);
    int x2 = analogRead(JOY2_X);
    int y2 = analogRead(JOY2_Y);
    
    joy1X_min = min(joy1X_min, x1);
    joy1X_max = max(joy1X_max, x1);
    joy1Y_min = min(joy1Y_min, y1);
    joy1Y_max = max(joy1Y_max, y1);
    joy2X_min = min(joy2X_min, x2);
    joy2X_max = max(joy2X_max, x2);
    joy2Y_min = min(joy2Y_min, y2);
    joy2Y_max = max(joy2Y_max, y2);
    
    delay(2);
  }
  
  joy1X_center = (joy1X_min + joy1X_max) / 2;
  joy1Y_center = (joy1Y_min + joy1Y_max) / 2;
  joy2X_center = (joy2X_min + joy2X_max) / 2;
  joy2Y_center = (joy2Y_min + joy2Y_max) / 2;
  
  calibrated = true;
  digitalWrite(LED_PIN, HIGH);
  
  Serial.println("Calibration done!");
}

int mapToAxis(int value, int minVal, int maxVal, int center, int deadZone) {
  if (value > center - deadZone && value < center + deadZone) {
    return 0;
  }
  if (value < center) {
    return map(value, minVal, center - deadZone, -127, 0);
  } else {
    return map(value, center + deadZone, maxVal, 0, 127);
  }
}