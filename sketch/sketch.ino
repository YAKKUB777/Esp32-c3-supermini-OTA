#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ====================== Піни для ESP32-C3 Supermini ======================
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8
#define TFT_LED     9

#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_SEL     3
#define BTN_BACK    0   // BOOT кнопка

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ====================== Стани меню ======================
enum MenuState {
  MAIN_MENU,
  SETTINGS
};
MenuState currentState = MAIN_MENU;
int menuIndex = 0;
int brightness = 150;

// ====================== Прототипи ======================
void drawMainMenu();
void drawSettings();
void checkButtons();

// ====================== Setup ======================
void setup() {
  Serial.begin(115200);
  
  // Кнопки
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  
  // Підсвітка
  pinMode(TFT_LED, OUTPUT);
  analogWrite(TFT_LED, brightness);
  
  // Дисплей
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);   // спробуйте INITR_GREENTAB або INITR_REDTAB, якщо не працює
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  
  drawMainMenu();
}

// ====================== Головний цикл ======================
void loop() {
  checkButtons();
  delay(20);
}

// ====================== Малювання меню ======================
void drawMainMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.println("RF TOOL v3.0");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
  
  const char* items[] = {"Wi-Fi Tools", "Sub-GHz Scan", "Sub-GHz Jam", "Settings"};
  for (int i = 0; i < 4; i++) {
    int y = 30 + i * 20;
    if (i == menuIndex) {
      tft.setTextColor(ST77XX_YELLOW);
      tft.setCursor(5, y);
      tft.print(">");
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }
    tft.setCursor(15, y);
    tft.print(items[i]);
  }
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(5, 150);
  tft.print("UP/DN SEL BACK");
}

void drawSettings() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.print("Settings");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 35);
  tft.print("Brightness: ");
  tft.print(map(brightness, 0, 255, 0, 100));
  tft.print("%");
}

// ====================== Обробка кнопок ======================
void checkButtons() {
  static unsigned long lastPress = 0;
  if (millis() - lastPress < 200) return;
  
  if (digitalRead(BTN_UP) == LOW) {
    lastPress = millis();
    if (currentState == MAIN_MENU) {
      menuIndex = (menuIndex + 3) % 4;
      drawMainMenu();
    } else if (currentState == SETTINGS) {
      brightness = constrain(brightness - 25, 0, 255);
      analogWrite(TFT_LED, brightness);
      drawSettings();
    }
  }
  
  if (digitalRead(BTN_DOWN) == LOW) {
    lastPress = millis();
    if (currentState == MAIN_MENU) {
      menuIndex = (menuIndex + 1) % 4;
      drawMainMenu();
    } else if (currentState == SETTINGS) {
      brightness = constrain(brightness + 25, 0, 255);
      analogWrite(TFT_LED, brightness);
      drawSettings();
    }
  }
  
  if (digitalRead(BTN_SEL) == LOW) {
    lastPress = millis();
    if (currentState == MAIN_MENU) {
      if (menuIndex == 3) {
        currentState = SETTINGS;
        drawSettings();
      }
    } else if (currentState == SETTINGS) {
      currentState = MAIN_MENU;
      drawMainMenu();
    }
  }
  
  if (digitalRead(BTN_BACK) == LOW) {
    lastPress = millis();
    if (currentState != MAIN_MENU) {
      currentState = MAIN_MENU;
      drawMainMenu();
    }
  }
}