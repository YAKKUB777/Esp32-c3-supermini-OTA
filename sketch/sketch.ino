/*
 * RF TOOL v4.1 - Fixed TFT + CC1101 for ESP32-C3 Supermini
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ====================== Піни ======================
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8

#define CC1101_CS   4
#define CC1101_MISO 21

#define BTN_UP      1
#define BTN_DOWN    20
#define BTN_SEL     3
#define BTN_BACK    0

// ====================== Кольори ======================
#define BG_COLOR        ST77XX_BLACK
#define HEADER_COLOR    ST77XX_CYAN
#define SELECTED_COLOR  ST77XX_YELLOW
#define TEXT_COLOR      ST77XX_WHITE
#define ALERT_COLOR     ST77XX_RED
#define SUCCESS_COLOR   ST77XX_GREEN
#define LINE_COLOR      ST77XX_BLUE

// ====================== Об'єкти ======================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
CC1101 cc1101 = new Module(CC1101_CS, RADIOLIB_NC, RADIOLIB_NC);

// ... (весь інший код залишається як у попередньому повідомленні, 
//      але з двома важливими змінами в setup())

void setup() {
  Serial.begin(115200);
  delay(500);

  // Кнопки
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  // *** ЗМІНА 1: Ініціалізація SPI без MISO (CC1101 поки що відключений) ***
  SPI.begin(TFT_SCLK, -1, TFT_MOSI);  // MISO = -1, CS не передаємо

  // *** ЗМІНА 2: Явне підняття CS для CC1101, щоб він не заважав ***
  pinMode(CC1101_CS, OUTPUT);
  digitalWrite(CC1101_CS, HIGH);

  // Дисплей
  tft.initR(INITR_MINI160x80);  // або той, що спрацював у тесті
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  tft.setTextWrap(false);
  
  // Показуємо, що дисплей працює
  tft.setTextColor(SUCCESS_COLOR);
  tft.setCursor(10, 30);
  tft.print("TFT OK!");

  // *** ЗМІНА 3: CC1101 ініціалізуємо окремо, з явним керуванням CS ***
  // RadioLib всередині викликає SPI.begin() з потрібними параметрами,
  // тому ми спочатку завершуємо попередню шину
  SPI.end();
  delay(10);
  SPI.begin(TFT_SCLK, CC1101_MISO, TFT_MOSI);
  digitalWrite(CC1101_CS, HIGH);
  
  int state = cc1101.begin(freqs[freqIndex]);
  if (state == RADIOLIB_ERR_NONE) {
    cc1101.setOutputPower(12);
    Serial.println("CC1101 initialized");
    tft.setCursor(10, 50);
    tft.print("CC1101 OK!");
  } else {
    Serial.printf("CC1101 init failed: %d\n", state);
    tft.setTextColor(ALERT_COLOR);
    tft.setCursor(10, 50);
    tft.print("CC1101 FAIL");
  }
  cc1101.standby();
  
  delay(1000);
  
  // Wi‑Fi
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_ps(WIFI_PS_NONE);

  drawMainMenu();
}

// ... (решта коду без змін)