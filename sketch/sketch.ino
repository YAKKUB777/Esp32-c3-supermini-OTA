#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <PN532.h>
#include "config.h"

// ====================== Ініціалізація ======================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Ініціалізація PN532 через I2C (Elechouse)
PN532 nfc(PN532_SDA, PN532_SCL);

// ====================== Глобальні змінні ======================
uint8_t savedUID[7] = {0};
uint8_t savedUIDlen = 0;
bool cardSaved = false;

// ====================== Прототипи ======================
void initDisplay();
void initPN532();
void drawMenu();
void showMessage(const char* msg, uint16_t color);
void scanUID();
void emulateUID();

// ====================== SETUP ======================
void setup() {
  Serial.begin(115200);
  
  pinMode(BTN_SCAN, INPUT_PULLUP);
  pinMode(BTN_EMULATE, INPUT_PULLUP);
  
  pinMode(TFT_LED, OUTPUT);
  analogWrite(TFT_LED, 200);
  initDisplay();
  
  initPN532();
  
  drawMenu();
}

void loop() {
  if (digitalRead(BTN_SCAN) == LOW) {
    delay(50);
    if (digitalRead(BTN_SCAN) == LOW) {
      scanUID();
      drawMenu();
    }
    while (digitalRead(BTN_SCAN) == LOW);
  }
  
  if (digitalRead(BTN_EMULATE) == LOW) {
    delay(50);
    if (digitalRead(BTN_EMULATE) == LOW) {
      if (cardSaved) {
        emulateUID();
      } else {
        showMessage("No card saved!", ST77XX_RED);
        delay(1000);
        drawMenu();
      }
    }
    while (digitalRead(BTN_EMULATE) == LOW);
  }
  
  delay(100);
}

void initDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
}

void initPN532() {
  nfc.begin();
  
  uint32_t version = nfc.getFirmwareVersion();
  if (!version) {
    showMessage("PN532 ERROR!", ST77XX_RED);
    while (1);
  }
  
  Serial.print("PN532 firmware: ");
  Serial.print((version >> 24) & 0xFF, HEX);
  Serial.print('.');
  Serial.println((version >> 16) & 0xFF, HEX);
  
  nfc.SAMConfig();
}

void drawMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(5, 5);
  tft.println("NFC TOOL");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
  
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 30);
  tft.println("BTN0: SCAN UID");
  tft.setCursor(5, 50);
  tft.println("BTN1: EMULATE");
  
  if (cardSaved) {
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(5, 80);
    tft.print("UID: ");
    for (int i = 0; i < savedUIDlen; i++) {
      if (savedUID[i] < 0x10) tft.print("0");
      tft.print(savedUID[i], HEX);
      if (i < savedUIDlen - 1) tft.print(":");
    }
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(5, 80);
    tft.print("No card saved");
  }
}

void showMessage(const char* msg, uint16_t color) {
  tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
  tft.setTextColor(color);
  tft.setCursor(5, 60);
  tft.print(msg);
  delay(1500);
}

void scanUID() {
  tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 60);
  tft.print("Place card...");
  tft.setCursor(10, 75);
  tft.print("near reader");
  
  uint8_t uid[7];
  uint8_t uidLen;
  
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 1000)) {
    showMessage("No card found!", ST77XX_RED);
    return;
  }
  
  savedUIDlen = uidLen;
  memcpy(savedUID, uid, uidLen);
  cardSaved = true;
  
  tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(5, 40);
  tft.print("UID READ!");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 60);
  tft.print("UID (");
  tft.print(uidLen);
  tft.print("):");
  tft.setCursor(5, 80);
  for (int i = 0; i < uidLen; i++) {
    if (uid[i] < 0x10) tft.print("0");
    tft.print(uid[i], HEX);
    if (i < uidLen - 1) tft.print(":");
  }
  
  delay(2000);
}

void emulateUID() {
  tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 60);
  tft.print("Emulating...");
  tft.setCursor(5, 80);
  for (int i = 0; i < savedUIDlen; i++) {
    if (savedUID[i] < 0x10) tft.print("0");
    tft.print(savedUID[i], HEX);
    if (i < savedUIDlen - 1) tft.print(":");
  }
  
  // Встановлюємо режим емуляції (Elechouse)
  if (!nfc.tgInitAsTarget(savedUID, savedUIDlen)) {
    showMessage("Emulation failed!", ST77XX_RED);
    return;
  }
  
  delay(5000);
  
  nfc.inReleaseTarget();
  showMessage("Emulation done", ST77XX_CYAN);
}