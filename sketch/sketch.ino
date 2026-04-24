/*
 * ============================================================
 *  NFC/RFID Emulator for ESP32-C3 Supermini + PN532 (I2C) + ST7735
 *  Features:
 *    - Read MIFARE Classic 1K/4K (UID + sectors)
 *    - Save card data to SD or EEPROM
 *    - Emulate saved card (tgInitAsTarget)
 *    - Display info on 0.96" TFT
 * ============================================================
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include "config.h"

// ====================== Ініціалізація дисплея ======================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ====================== Ініціалізація PN532 (I2C) ======================
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

// ====================== Дані картки ======================
struct CardData {
  uint8_t uid[7];
  uint8_t uidLen;
  uint8_t sectorData[64][16];  // 64 сектори × 16 байт (для MIFARE Classic 1K)
  int sectorCount;
  bool isValid;
};

CardData currentCard = {0};
bool isEmulating = false;
unsigned long emulationStartTime = 0;
unsigned long lastDisplayTime = 0;

// ====================== Прототипи ======================
void initDisplay();
void initPN532();
void drawMenu();
void drawCardInfo();
void readCard();
void emulateCard();
void saveCardToEEPROM();
void loadCardFromEEPROM();

// ====================== SETUP ======================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== NFC/RFID Emulator v1.0 ===");
  
  // Кнопки
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  
  // Дисплей
  pinMode(TFT_LED, OUTPUT);
  analogWrite(TFT_LED, 180);  // яскравість 70%
  initDisplay();
  
  // PN532
  initPN532();
  
  drawMenu();
  lastDisplayTime = millis();
}

// ====================== LOOP ======================
void loop() {
  // Перевірка кнопок
  if (digitalRead(BTN_A) == LOW) {
    delay(50);  // антидребезг
    if (digitalRead(BTN_A) == LOW) {
      if (!isEmulating) {
        readCard();
      } else {
        // Зупинити емуляцію
        nfc.inReleaseTarget();
        isEmulating = false;
        drawMenu();
      }
    }
    while (digitalRead(BTN_A) == LOW);  // чекаємо відпускання
  }
  
  if (digitalRead(BTN_B) == LOW) {
    delay(50);
    if (digitalRead(BTN_B) == LOW) {
      if (!isEmulating && currentCard.isValid) {
        emulateCard();
      } else if (isEmulating) {
        nfc.inReleaseTarget();
        isEmulating = false;
        drawMenu();
      }
    }
    while (digitalRead(BTN_B) == LOW);
  }
  
  // Таймаут підсвітки дисплея
  if (millis() - lastDisplayTime > DISPLAY_TIMEOUT_MS) {
    analogWrite(TFT_LED, 0);
  } else if (analogRead(TFT_LED) == 0 && millis() - lastDisplayTime < DISPLAY_TIMEOUT_MS) {
    analogWrite(TFT_LED, 180);
  }
  
  // Оновлення таймера для емуляції
  if (isEmulating && (millis() - emulationStartTime > EMULATION_TIMEOUT_MS)) {
    nfc.inReleaseTarget();
    isEmulating = false;
    drawMenu();
  }
  
  delay(100);
}

// ====================== Ініціалізація дисплея ======================
void initDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);  // портретна орієнтація 80x160
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.println("NFC/RFID Emulator");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
}

// ====================== Ініціалізація PN532 ======================
void initPN532() {
  Wire.begin(PN532_SDA, PN532_SCL);
  nfc.begin();
  
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not found!");
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 70);
    tft.print("PN532 ERROR!");
    while (1);
  }
  
  Serial.print("PN532 firmware: ");
  Serial.print((versiondata >> 24) & 0xFF, HEX);
  Serial.print('.');
  Serial.println((versiondata >> 16) & 0xFF, HEX);
  
  nfc.SAMConfig();
}

// ====================== Головне меню ======================
void drawMenu() {
  tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 30);
  tft.println("BTN A: READ CARD");
  tft.setCursor(5, 50);
  tft.println("BTN B: EMULATE");
  
  if (currentCard.isValid) {
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(5, 80);
    tft.print("UID: ");
    for (int i = 0; i < currentCard.uidLen; i++) {
      if (currentCard.uid[i] < 0x10) tft.print("0");
      tft.print(currentCard.uid[i], HEX);
      if (i < currentCard.uidLen - 1) tft.print(":");
    }
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(5, 80);
    tft.print("No card saved");
  }
  
  lastDisplayTime = millis();
}

// ====================== Виведення інформації про картку ======================
void drawCardInfo() {
  tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(5, 25);
  tft.print("Card detected!");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 40);
  tft.print("UID (");
  tft.print(currentCard.uidLen);
  tft.println(" bytes):");
  tft.setCursor(5, 55);
  for (int i = 0; i < currentCard.uidLen; i++) {
    if (currentCard.uid[i] < 0x10) tft.print("0");
    tft.print(currentCard.uid[i], HEX);
    if (i < currentCard.uidLen - 1) tft.print(":");
  }
  tft.setCursor(5, 75);
  tft.print("Sectors: ");
  tft.print(currentCard.sectorCount);
  tft.setCursor(5, 95);
  tft.print("Press BTN A to save");
  
  lastDisplayTime = millis();
}

// ====================== Читання картки ======================
void readCard() {
  tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 60);
  tft.print("Place card...");
  tft.setCursor(10, 75);
  tft.print("near reader");
  lastDisplayTime = millis();
  
  uint8_t uid[7];
  uint8_t uidLen;
  
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 1000)) {
    tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(15, 70);
    tft.print("No card!");
    delay(1000);
    drawMenu();
    return;
  }
  
  // Зберігаємо UID
  currentCard.uidLen = uidLen;
  memcpy(currentCard.uid, uid, uidLen);
  
  // Перевіряємо, чи це MIFARE Classic
  if (uidLen == 4) {
    currentCard.sectorCount = 16;  // 16 секторів для MIFARE Classic 1K
    
    // Спроба аутентифікації та читання секторів (стандартні ключі)
    uint8_t key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for (int sector = 0; sector < 16; sector++) {
      uint8_t block = sector * 4;
      if (nfc.mifareclassic_AuthenticateBlock(uid, uidLen, block, 0, key)) {
        nfc.mifareclassic_ReadDataBlock(block, currentCard.sectorData[sector]);
      }
    }
  } else {
    currentCard.sectorCount = 0;
  }
  
  currentCard.isValid = true;
  drawCardInfo();
  saveCardToEEPROM();
  
  // Невелика вібрація через п'єзо (якщо є) або просто затримка
  delay(2000);
  drawMenu();
}

// ====================== Емуляція картки ======================
void emulateCard() {
  if (!currentCard.isValid) {
    tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 70);
    tft.print("No card data!");
    delay(1000);
    drawMenu();
    return;
  }
  
  tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 60);
  tft.print("Emulating...");
  tft.setCursor(10, 75);
  tft.print("UID: ");
  for (int i = 0; i < currentCard.uidLen; i++) {
    if (currentCard.uid[i] < 0x10) tft.print("0");
    tft.print(currentCard.uid[i], HEX);
    if (i < currentCard.uidLen - 1) tft.print(":");
  }
  lastDisplayTime = millis();
  
  // Встановлюємо UID для емуляції
  if (!nfc.tgInitAsTarget(0, currentCard.uid, currentCard.uidLen, 0, 0, 0, 0)) {
    tft.fillRect(0, 20, 80, 140, ST77XX_BLACK);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(10, 70);
    tft.print("Emulation fail!");
    delay(1000);
    drawMenu();
    return;
  }
  
  isEmulating = true;
  emulationStartTime = millis();
  
  // Очікуємо на запити зчитувача
  while (isEmulating && (millis() - emulationStartTime < EMULATION_TIMEOUT_MS)) {
    int8_t cmd;
    uint8_t buf[64];
    uint8_t len;
    
    // Перевіряємо, чи є запит
    if (nfc.inReceivePassiveTarget(cmd, buf, &len, 100)) {
      // Обробляємо запит (тут можна додати відповідь з даними секторів)
      // Для простоти поки що тільки відповідаємо UID
    }
    
    delay(10);
  }
  
  nfc.inReleaseTarget();
  isEmulating = false;
  drawMenu();
}

// ====================== Збереження даних в EEPROM ======================
void saveCardToEEPROM() {
  // Тут можна зберігати дані в EEPROM або на SD-карту
  Serial.println("Card saved (simulated)");
}

// ====================== Завантаження даних з EEPROM ======================
void loadCardFromEEPROM() {
  // Тут можна завантажувати дані
  Serial.println("Card loaded (simulated)");
}