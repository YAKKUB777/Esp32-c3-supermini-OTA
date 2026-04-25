/*
 * NFC/RFID Емулятор на ESP32-C3 SuperMini
 * Дисплей: ST7735 0.96" 80x160
 * NFC: PN532 (I2C)
 * Кнопки: SCAN (GPIO0), EMULATE (GPIO1)
 */

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include "config.h"

#define C_BG     ST77XX_BLACK
#define C_HEAD   ST77XX_CYAN
#define C_TEXT   ST77XX_WHITE
#define C_GREEN  ST77XX_GREEN
#define C_RED    ST77XX_RED
#define C_YELLOW ST77XX_YELLOW
#define C_DIM    0x4208

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

uint8_t savedUID[MAX_UID_LEN];
uint8_t savedUIDLen = 0;
bool    hasUID      = false;

void drawHeader(const char* title);
void drawFooter(const char* hints);
void drawMenu();
void drawUID(uint8_t* uid, uint8_t len, uint16_t color);
void showMessage(const char* line1, const char* line2, uint16_t color);
void scanUID();
void emulateUID();

void drawHeader(const char* title) {
  tft.fillRect(0, 0, 160, 14, C_HEAD);
  tft.setTextColor(C_BG);
  tft.setTextSize(1);
  int x = (160 - strlen(title) * 6) / 2;
  tft.setCursor(x < 0 ? 0 : x, 3);
  tft.print(title);
}

void drawFooter(const char* hints) {
  tft.fillRect(0, 69, 160, 11, 0x1082);
  tft.setTextColor(C_DIM);
  tft.setTextSize(1);
  tft.setCursor(2, 71);
  tft.print(hints);
}

void drawUID(uint8_t* uid, uint8_t len, uint16_t color) {
  String hex = "";
  for (uint8_t i = 0; i < len; i++) {
    if (uid[i] < 0x10) hex += "0";
    hex += String(uid[i], HEX);
    if (i < len - 1) hex += ":";
  }
  hex.toUpperCase();
  tft.setTextColor(color);
  tft.setTextSize(1);
  if (hex.length() > 17) {
    tft.setCursor(2, 38);
    tft.print(hex.substring(0, 17));
    tft.setCursor(2, 49);
    tft.print(hex.substring(17));
  } else {
    tft.setCursor(2, 44);
    tft.print(hex);
  }
}

void showMessage(const char* line1, const char* line2, uint16_t color) {
  tft.fillRect(0, 14, 160, 56, C_BG);
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.setCursor(2, 25);
  tft.print(line1);
  if (line2 && strlen(line2) > 0) {
    tft.setTextColor(C_TEXT);
    tft.setCursor(2, 40);
    tft.print(line2);
  }
}

void drawMenu() {
  tft.fillScreen(C_BG);
  drawHeader("[ NFC EMULATOR ]");
  tft.setTextColor(C_DIM);
  tft.setTextSize(1);
  tft.setCursor(2, 17);
  tft.print("Saved UID:");
  if (hasUID) {
    drawUID(savedUID, savedUIDLen, C_YELLOW);
    tft.setTextColor(C_DIM);
    tft.setCursor(2, 59);
    tft.print("Len: ");
    tft.setTextColor(C_TEXT);
    tft.print(savedUIDLen);
    tft.print(" bytes");
  } else {
    tft.setTextColor(C_RED);
    tft.setCursor(2, 38);
    tft.print("-- none --");
    tft.setTextColor(C_DIM);
    tft.setCursor(2, 52);
    tft.print("Press SCAN to read");
  }
  drawFooter("SCAN:read  EMU:emulate");
}

void scanUID() {
  tft.fillScreen(C_BG);
  drawHeader("[ SCAN ]");
  showMessage("Place card near", "reader...", C_HEAD);
  drawFooter("Waiting...");

  uint8_t uid[MAX_UID_LEN];
  uint8_t uidLen = 0;
  bool found = false;
  unsigned long start = millis();

  while (millis() - start < 10000) {
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen)) {
      found = true;
      break;
    }
    delay(100);
  }

  if (found && uidLen > 0) {
    memcpy(savedUID, uid, uidLen);
    savedUIDLen = uidLen;
    hasUID = true;

    tft.fillScreen(C_BG);
    drawHeader("[ SCAN ]");
    tft.setTextColor(C_GREEN);
    tft.setTextSize(1);
    tft.setCursor(2, 17);
    tft.print("Card found!");
    tft.setTextColor(C_DIM);
    tft.setCursor(2, 28);
    tft.print("UID:");
    drawUID(savedUID, savedUIDLen, C_YELLOW);
    tft.setTextColor(C_DIM);
    tft.setCursor(2, 60);
    tft.print("Len: ");
    tft.setTextColor(C_TEXT);
    tft.print(savedUIDLen);
    tft.print(" bytes");
    drawFooter("Saved!");
    delay(2500);
  } else {
    tft.fillScreen(C_BG);
    drawHeader("[ SCAN ]");
    showMessage("No card found!", "Timeout 10s", C_RED);
    drawFooter("Press any button");
    delay(2000);
  }

  drawMenu();
}
void emulateUID() {
  if (!hasUID) {
    tft.fillScreen(C_BG);
    drawHeader("[ EMULATE ]");
    showMessage("No UID saved!", "Scan card first", C_RED);
    drawFooter("Press any button");
    delay(2000);
    drawMenu();
    return;
  }

  tft.fillScreen(C_BG);
  drawHeader("[ EMULATE ]");
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(2, 17);
  tft.print("Emulating...");
  tft.setTextColor(C_DIM);
  tft.setCursor(2, 28);
  tft.print("UID:");
  drawUID(savedUID, savedUIDLen, C_YELLOW);
  drawFooter("Hold near reader...");

  uint8_t params[32];
  uint8_t p = 0;
  params[p++] = 0x00; // ATQA byte 1
  params[p++] = 0x04; // ATQA byte 2
  params[p++] = 0x08; // SAK — Mifare Classic 1K
  memcpy(&params[p], savedUID, savedUIDLen);
  p += savedUIDLen;

  unsigned long startTime = millis();

  while (millis() - startTime < EMULATE_TIME) {
    // Оновлюємо таймер
    uint32_t remaining = (EMULATE_TIME - (millis() - startTime)) / 1000;
    tft.fillRect(2, 60, 80, 10, C_BG);
    tft.setTextColor(C_TEXT);
    tft.setTextSize(1);
    tft.setCursor(2, 60);
    tft.print("Time: ");
    tft.print(remaining);
    tft.print("s");

    // Ініціалізуємось як ціль
    int8_t result = nfc.tgInitAsTarget(params, p, 1000);

    if (result > 0) {
      // Рідер підключився — обробляємо команди
      uint8_t cmd[32];
      uint8_t cmdLen = 0;

      // Читаємо команду від рідера
      bool got = nfc.tgGetData(cmd, &cmdLen);

      if (got && cmdLen > 0) {
        // Показуємо що отримали команду
        tft.fillRect(2, 60, 156, 10, C_BG);
        tft.setTextColor(C_YELLOW);
        tft.setCursor(2, 60);
        tft.print("CMD: 0x");
        tft.print(cmd[0], HEX);

        // Обробка команд рідера
        if (cmd[0] == 0x60 || cmd[0] == 0x61) {
          // Auth Key A або Key B — відповідаємо ACK (0x00)
          uint8_t ack[] = {0x00};
          nfc.tgSetData(ack, 1);

        } else if (cmd[0] == 0x30) {
          // Read block — повертаємо 16 нульових байт + CRC
          uint8_t block[16];
          memset(block, 0x00, sizeof(block));
          nfc.tgSetData(block, 16);

        } else if (cmd[0] == 0xA0 || cmd[0] == 0xA2) {
          // Write block — підтверджуємо ACK
          uint8_t ack[] = {0x00};
          nfc.tgSetData(ack, 1);

        } else if (cmd[0] == 0x50) {
          // HALT — рідер хоче завершити сесію
          // просто виходимо з циклу команд
          break;

        } else {
          // Невідома команда — відповідаємо NAK
          uint8_t nak[] = {0x05};
          nfc.tgSetData(nak, 1);
        }
      }
    }

    // Перевіряємо кнопку для виходу
    if (digitalRead(BTN_SCAN) == LOW || digitalRead(BTN_EMULATE) == LOW) {
      delay(50);
      break;
    }
  }

  nfc.inRelease(0);

  tft.fillScreen(C_BG);
  drawHeader("[ EMULATE ]");
  showMessage("Done!", "Emulation stopped", C_GREEN);
  drawFooter("Press any button");
  delay(1500);
  drawMenu();
}


void setup() {
  Serial.begin(115200);

  pinMode(BTN_SCAN,    INPUT_PULLUP);
  pinMode(BTN_EMULATE, INPUT_PULLUP);

  SPI.begin(TFT_SCK, -1, TFT_MOSI);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(C_BG);

  tft.setTextColor(C_HEAD);
  tft.setTextSize(2);
  tft.setCursor(10, 15);
  tft.print("NFC EMU");
  tft.setTextColor(C_DIM);
  tft.setTextSize(1);
  tft.setCursor(15, 40);
  tft.print("ESP32-C3");
  tft.setCursor(15, 52);
  tft.print("Initializing...");

  Wire.begin(PN532_SDA, PN532_SCL);
  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    tft.fillScreen(C_BG);
    tft.setTextColor(C_RED);
    tft.setTextSize(1);
    tft.setCursor(2, 20);
    tft.print("PN532 not found!");
    tft.setTextColor(C_DIM);
    tft.setCursor(2, 35);
    tft.print("SDA=GPIO20");
    tft.setCursor(2, 47);
    tft.print("SCL=GPIO21");
    while (1) delay(1000);
  }

  tft.fillRect(0, 40, 160, 20, C_BG);
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(15, 42);
  tft.print("PN532 OK! v");
  tft.print((versiondata >> 16) & 0xFF);
  tft.print(".");
  tft.print((versiondata >> 8) & 0xFF);

  nfc.SAMConfig();
  delay(1200);
  drawMenu();
}

void loop() {
  if (digitalRead(BTN_SCAN) == LOW) {
    delay(50);
    if (digitalRead(BTN_SCAN) == LOW) {
      while (digitalRead(BTN_SCAN) == LOW) delay(10);
      scanUID();
    }
  }

  if (digitalRead(BTN_EMULATE) == LOW) {
    delay(50);
    if (digitalRead(BTN_EMULATE) == LOW) {
      while (digitalRead(BTN_EMULATE) == LOW) delay(10);
      emulateUID();
    }
  }

  delay(10);
}