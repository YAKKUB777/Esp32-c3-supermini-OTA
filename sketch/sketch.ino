#include <BleGamepad.h>
#include <esp_adc_cal.h>

// ====================== Аналогові піни (ADC) ======================
#define JOY1_X    1   // GPIO1 - ADC1_CH0
#define JOY1_Y    2   // GPIO2 - ADC1_CH1
#define JOY2_X    3   // GPIO3 - ADC1_CH2
#define JOY2_Y    4   // GPIO4 - ADC1_CH3

// ====================== Цифрові піни (кнопки) ======================
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
int joy1X_center = 0, joy1Y_center = 0;
int joy2X_center = 0, joy2Y_center = 0;
int joy1X_min = 4095, joy1X_max = 0;
int joy1Y_min = 4095, joy1Y_max = 0;
int joy2X_min = 4095, joy2X_max = 0;
int joy2Y_min = 4095, joy2Y_max = 0;

// Фільтрація
const int FILTER_SIZE = 5;
int joy1X_buffer[FILTER_SIZE];
int joy1Y_buffer[FILTER_SIZE];
int joy2X_buffer[FILTER_SIZE];
int joy2Y_buffer[FILTER_SIZE];
int bufIndex = 0;

void calibrate();
int medianFilter(int* buffer, int newValue);
void readJoysticks(int &jx1, int &jy1, int &jx2, int &jy2);
void readButtons(uint8_t &btnMask);
int mapToAxis(int value, int minVal, int maxVal, int center, int deadZone);

void setup() {
  Serial.begin(115200);
  
  // Налаштування цифрових пінів
  pinMode(JOY1_SW, INPUT_PULLUP);
  pinMode(JOY2_SW, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_X, INPUT_PULLUP);
  pinMode(BTN_Y, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Налаштування АЦП (12 біт, 0-4095)
  analogReadResolution(12);
  
  // Калібрування АЦП для більш точних вимірювань
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  
  // Запуск BLE геймпада
  bleGamepad.begin();
  while (!bleGamepad.isConnected()) {
    delay(100);
  }
  digitalWrite(LED_PIN, HIGH);
  
  calibrate();
}

void loop() {
  if (!bleGamepad.isConnected()) {
    digitalWrite(LED_PIN, LOW);
    delay(100);
    return;
  }
  
  digitalWrite(LED_PIN, HIGH);
  
  int jx1, jy1, jx2, jy2;
  readJoysticks(jx1, jy1, jx2, jy2);
  
  uint8_t buttons = 0;
  readButtons(buttons);
  
  bleGamepad.press(buttons);
  bleGamepad.setAxes(jx1, jy1, jx2, jy2, 0, 0, 0, 0);
  
  delay(5);
}

void calibrate() {
  Serial.println("Calibrating...");
  
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
  
  Serial.println("Calibration done!");
  Serial.print("Joy1 X: min="); Serial.print(joy1X_min); Serial.print(" max="); Serial.print(joy1X_max); Serial.print(" center="); Serial.println(joy1X_center);
  Serial.print("Joy1 Y: min="); Serial.print(joy1Y_min); Serial.print(" max="); Serial.print(joy1Y_max); Serial.print(" center="); Serial.println(joy1Y_center);
  Serial.print("Joy2 X: min="); Serial.print(joy2X_min); Serial.print(" max="); Serial.print(joy2X_max); Serial.print(" center="); Serial.println(joy2X_center);
  Serial.print("Joy2 Y: min="); Serial.print(joy2Y_min); Serial.print(" max="); Serial.print(joy2Y_max); Serial.print(" center="); Serial.println(joy2Y_center);
}

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

void readJoysticks(int &jx1, int &jy1, int &jx2, int &jy2) {
  int rawX1 = analogRead(JOY1_X);
  int rawY1 = analogRead(JOY1_Y);
  int rawX2 = analogRead(JOY2_X);
  int rawY2 = analogRead(JOY2_Y);
  
  int x1 = medianFilter(joy1X_buffer, rawX1);
  int y1 = medianFilter(joy1Y_buffer, rawY1);
  int x2 = medianFilter(joy2X_buffer, rawX2);
  int y2 = medianFilter(joy2Y_buffer, rawY2);
  
  int deadZone = (joy1X_max - joy1X_min) / 20;
  
  jx1 = mapToAxis(x1, joy1X_min, joy1X_max, joy1X_center, deadZone);
  jy1 = mapToAxis(y1, joy1Y_min, joy1Y_max, joy1Y_center, deadZone);
  jx2 = mapToAxis(x2, joy2X_min, joy2X_max, joy2X_center, deadZone);
  jy2 = mapToAxis(y2, joy2Y_min, joy2Y_max, joy2Y_center, deadZone);
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

void readButtons(uint8_t &btnMask) {
  btnMask = 0;
  if (digitalRead(JOY1_SW) == LOW) btnMask |= 0x01;
  if (digitalRead(JOY2_SW) == LOW) btnMask |= 0x02;
  if (digitalRead(BTN_A) == LOW)   btnMask |= 0x04;
  if (digitalRead(BTN_B) == LOW)   btnMask |= 0x08;
  if (digitalRead(BTN_X) == LOW)   btnMask |= 0x10;
  if (digitalRead(BTN_Y) == LOW)   btnMask |= 0x20;
}