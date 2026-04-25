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

// ===== КОЛЬОРИ =====
#define C_BG     ST77XX_BLACK
#define C_HEAD   ST77XX_CYAN
#define C_TEXT   ST77XX_WHITE
#define C_GREEN  ST77XX_GREEN
#define C_RED    ST77XX_RED
#define C_YELLOW ST77XX_YELLOW
#define C_DIM    0x4208

// ===== ОБ'ЄКТИ =====
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
PN532_I2C pn532i2c(Wire);
PN532 nfc(pn532i2c);

// ===== ГЛОБАЛЬНІ ЗМІННІ =====
uint8_t savedUID[MAX_UID_LEN];
uint8_t savedUIDLen = 0;
bool    hasUID      = false;

// ===== ПРОТОТИПИ =====
void drawHeader(const char* title);
void drawFooter(const char* hints);
void drawMenu();
void drawUID(uint8_t* uid, uint8_t len, uint16_t color);
void showMessage(const char* line1, const char* line2, uint16_t color);
void scanUID();
void emulateUID();
String uidToHex(uint8_t* uid, uint8_t len);

// ===== ДОПОМІЖНІ ФУНКЦІЇ =====

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

String uidToHex(uint8_t* uid, uint8_t len) {
  String result = "";
  for (uint8_t i = 0; i < len; i++) {
    if (uid[i] < 0x10) result += "0";
    result += String(uid[i], HEX);
    if (i < len - 1) result += ":";
  }
  result.toUpperCase();
  return result;
}

void drawUID(uint8_t* uid, uint8_t len, uint16_t color) {
  tft.setTextColor(color);
  tft.setTextSize(1);
  String hex = uidToHex(uid, len);
  // Якщо UID довгий — розбиваємо на два рядки
  if (hex.length() > 17) {
    String part1 = hex.substring(0, 17);
    String part2 = hex.substring(17);
    tft.setCursor(2, 38);
    tft.print(part1);
    tft.setCursor(2, 49);
    tft.print(part2);
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

// ===== ГОЛОВНЕ МЕНЮ =====
void drawMenu() {
  tft.fillScreen(C_BG);
  drawHeader("[ NFC EMULATOR ]");

  // Збережений UID
  tft.setTextColor(C_DIM);
  tft.setTextSize(1);
  tft.setCursor(2, 17);
  tft.print("Saved UID:");

  if (hasUID) {
    drawUID(savedUID, savedUIDLen, C_YELLOW);
    // Довжина
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

// ===== СКАНУВАННЯ UID =====
void scanUID() {
  tft.fillScreen(C_BG);
  drawHeader("[ SCAN ]");
  showMessage("Place card near", "reader...", C_HEAD);
  drawFooter("Waiting...");

  uint8_t uid[MAX_UID_LEN];
  uint8_t uidLen = 0;

  // Чекаємо картку до 10 секунд
  unsigned long start = millis();
  bool found = false;

  while (millis() - start < 10000) {
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen)) {
      found = true;
      break;
    }
    delay(100);
  }

  if (found && uidLen > 0) {
    // Зберігаємо UID
    memcpy(savedUID, uid, uidLen);
    savedUIDLen = uidLen;
    hasUID = true;

    // Показуємо результат
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
    drawFooter("Saved! Press any btn");
    delay(2500);
  } else {
    // Таймаут
    tft.fillScreen(C_BG);
    drawHeader("[ SCAN ]");
    showMessage("No card found!", "Timeout 10s", C_RED);
    drawFooter("Press any button");
    delay(2000);
  }

  drawMenu();
}

// ===== ЕМУЛЯЦІЯ UID =====
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

  /*
   * Емуляція картки через tgInitAsTarget
   * PN532 представляється як ISO14443A картка з нашим UID
   */

  // Будуємо параметри для tgInitAsTarget
  // Mifare 1K emulation params
  uint8_t params[14 + MAX_UID_LEN] = {
    0x00,       // SENS_RES byte 1 (ATQA) - Mifare 1K
    0x04,       // SENS_RES byte 2 (ATQA)
    0x00,       // SEL_RES (SAK) - не завершений вибір (UID не повний)
  };

  // Копіюємо UID в параметри
  uint8_t paramsLen = 3;
  params[paramsLen++] = savedUIDLen;
  memcpy(&params[paramsLen], savedUID, savedUIDLen);
  paramsLen += savedUIDLen;

  unsigned long startTime = millis();
  unsigned long remaining = EMULATE_TIME;

  while (millis() - startTime < EMULATE_TIME) {
    // Оновлюємо таймер на дисплеї
    remaining = EMULATE_TIME - (millis() - startTime);
    tft.fillRect(2, 60, 100, 10, C_BG);
    tft.setTextColor(C_TEXT);
    tft.setTextSize(1);
    tft.setCursor(2, 60);
    tft.print("Time: ");
    tft.print(remaining / 1000);
    tft.print("s");

    // Спроба ініціалізуватись як ціль
    uint8_t responseLength = 0;
    uint8_t response[20];

    bool success = nfc.tgInitAsTarget(
      params,
      paramsLen,
      response,
      &responseLength,
      500  // timeout 500ms
    );

    if (success) {
      // Відповідаємо на команди рідера
      // Для простої емуляції просто чекаємо — PN532 сам обробляє базовий обмін
      delay(100);
    } else {
      delay(50);
    }

    // Перевіряємо кнопку для виходу
    if (digitalRead(BTN_SCAN) == LOW || digitalRead(BTN_EMULATE) == LOW) {
      delay(50);
      break;
    }
  }

  // Зупиняємо емуляцію
  nfc.inRelease(0);

  // Показуємо результат
  tft.fillScreen(C_BG);
  drawHeader("[ EMULATE ]");
  showMessage("Done!", "Emulation stopped", C_GREEN);
  drawFooter("Press any button");
  delay(1500);
  drawMenu();
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);

  // Кнопки
  pinMode(BTN_SCAN,    INPUT_PULLUP);
  pinMode(BTN_EMULATE, INPUT_PULLUP);

  // SPI для дисплею
  SPI.begin(TFT_SCK, -1, TFT_MOSI);

  // Дисплей
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(C_BG);

  // Splash screen
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

  // I2C для PN532
  Wire.begin(PN532_SDA, PN532_SCL);
  nfc.begin();

  // Перевірка PN532
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    tft.fillScreen(C_BG);
    tft.setTextColor(C_RED);
    tft.setTextSize(1);
    tft.setCursor(2, 20);
    tft.print("PN532 not found!");
    tft.setCursor(2, 35);
    tft.print("Check wiring:");
    tft.setTextColor(C_DIM);
    tft.setCursor(2, 50);
    tft.print("SDA=GPIO20");
    tft.setCursor(2, 62);
    tft.print("SCL=GPIO21");
    // Зависаємо — без NFC нема сенсу продовжувати
    while (1) delay(1000);
  }

  // PN532 знайдено — показуємо версію
  tft.fillRect(0, 50, 160, 30, C_BG);
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(15, 52);
  tft.print("PN532 OK! v");
  tft.print((versiondata >> 16) & 0xFF);
  tft.print(".");
  tft.print((versiondata >> 8) & 0xFF);

  // Налаштовуємо PN532 для читання карток ISO14443A
  nfc.SAMConfig();

  delay(1200);
  drawMenu();
}

// ===== LOOP =====
void loop() {
  if (digitalRead(BTN_SCAN) == LOW) {
    delay(50); // антидребезг
    if (digitalRead(BTN_SCAN) == LOW) {
      while (digitalRead(BTN_SCAN) == LOW) delay(10); // чекаємо відпускання
      scanUID();
    }
  }

  if (digitalRead(BTN_EMULATE) == LOW) {
    delay(50); // антидребезг
    if (digitalRead(BTN_EMULATE) == LOW) {
      while (digitalRead(BTN_EMULATE) == LOW) delay(10);
      emulateUID();
    }
  }

  delay(10);
}

