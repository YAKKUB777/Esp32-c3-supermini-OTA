#include <BleGamepad.h>

// ====================== Піни ======================
// Аналогові (ADC)
#define JOY1_X    1
#define JOY1_Y    2
#define JOY2_X    3
#define JOY2_Y    4

// Цифрові (кнопки)
#define JOY1_SW   5
#define JOY2_SW   6
#define BTN_A     7
#define BTN_B     8
#define BTN_X     9
#define BTN_Y     10
#define LED_PIN   20

// ====================== Налаштування BLE ======================
BleGamepad bleGamepad("ESP32 Gamepad", "ESP", 100);

// ====================== Калібрування ======================
int joy1X_min = 4095, joy1X_max = 0;
int joy1Y_min = 4095, joy1Y_max = 0;
int joy2X_min = 4095, joy2X_max = 0;
int joy2Y_min = 4095, joy2Y_max = 0;
int joy1X_center, joy1Y_center;
int joy2X_center, joy2Y_center;
bool calibrated = false;

// ====================== Фільтрація ======================
const int FILTER_SIZE = 5;
int joy1X_buf[FILTER_SIZE], joy1Y_buf[FILTER_SIZE];
int joy2X_buf[FILTER_SIZE], joy2Y_buf[FILTER_SIZE];
int bufIndex = 0;

int medianFilter(int* buffer, int newValue) {
  static int values[FILTER_SIZE];
  values[bufIndex] = newValue;
  bufIndex = (bufIndex + 1) % FILTER_SIZE;
  
  int sorted[FILTER_SIZE];
  memcpy(sorted, values, sizeof(values));
  for (int i = 0; i < FILTER_SIZE - 1; i++) {
    for (int j = i + 1; j < FILTER_SIZE; j++) {
      if (sorted[i] > sorted[j]) {
        int temp = sorted[i];
        sorted[i] = sorted[j];
        sorted[j] = temp;
      }
    }
  }
  return sorted[FILTER_SIZE / 2];
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

void calibrate() {
  Serial.println("Calibrating... Move joysticks to edges");
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
  
  calibrate();
  
  bleGamepad.begin();
  while (!bleGamepad.isConnected()) {
    delay(100);
  }
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  if (!bleGamepad.isConnected()) {
    digitalWrite(LED_PIN, LOW);
    delay(100);
    return;
  }
  
  digitalWrite(LED_PIN, HIGH);
  
  int rawX1 = analogRead(JOY1_X);
  int rawY1 = analogRead(JOY1_Y);
  int rawX2 = analogRead(JOY2_X);
  int rawY2 = analogRead(JOY2_Y);
  
  int x1 = medianFilter(joy1X_buf, rawX1);
  int y1 = medianFilter(joy1Y_buf, rawY1);
  int x2 = medianFilter(joy2X_buf, rawX2);
  int y2 = medianFilter(joy2Y_buf, rawY2);
  
  int deadZone = (joy1X_max - joy1X_min) / 20;
  
  int jx1 = mapToAxis(x1, joy1X_min, joy1X_max, joy1X_center, deadZone);
  int jy1 = mapToAxis(y1, joy1Y_min, joy1Y_max, joy1Y_center, deadZone);
  int jx2 = mapToAxis(x2, joy2X_min, joy2X_max, joy2X_center, deadZone);
  int jy2 = mapToAxis(y2, joy2Y_min, joy2Y_max, joy2Y_center, deadZone);
  
  uint8_t buttons = 0;
  if (digitalRead(JOY1_SW) == LOW) buttons |= 0x01;
  if (digitalRead(JOY2_SW) == LOW) buttons |= 0x02;
  if (digitalRead(BTN_A) == LOW)   buttons |= 0x04;
  if (digitalRead(BTN_B) == LOW)   buttons |= 0x08;
  if (digitalRead(BTN_X) == LOW)   buttons |= 0x10;
  if (digitalRead(BTN_Y) == LOW)   buttons |= 0x20;
  
  bleGamepad.setAxes(jx1, jy1, jx2, jy2, 0, 0, 0, 0);
  bleGamepad.press(buttons);
  
  delay(5);
}