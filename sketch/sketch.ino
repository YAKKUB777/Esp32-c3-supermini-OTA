/*
 * NRF24 Spectrum Scanner
 * ESP32-C3 Supermini + ST7735 0.96" + NRF24L01
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <RF24.h>
#include "config.h"

// ===== Об'єкти =====
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);
RF24 radio(NRF_CE, NRF_CSN);

// ===== Дані =====
int channels[126];
int peaks[126];
int curCh = 0;
int curRSSI = -90;
bool nrfOK = false;
unsigned long lastDraw = 0;

// ===== Прототипи =====
void initNRF();
void showInitScreen();
void drawSpectrum();

// =============================================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Ініціалізація дисплея
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  tft.fillScreen(C_BG);
  tft.setTextColor(C_OK);
  tft.setCursor(10, 10);
  tft.print("Display: OK");
  
  // Ініціалізація NRF24
  initNRF();
  
  // Масиви
  for (int i = 0; i < 126; i++) {
    channels[i] = -90;
    peaks[i] = -90;
  }
  
  // Показати статус
  showInitScreen();
  
  if (!nrfOK) {
    while(1) delay(1000);
  }
  
  delay(2000);
  tft.fillScreen(C_BG);
}

// =============================================
void loop() {
  // Сканування одного каналу
  SPI.end();
  delay(10);
  SPI.begin(TFT_SCLK, NRF_MISO, TFT_MOSI, NRF_CSN);
  
  radio.powerUp();
  radio.setChannel(curCh);
  radio.startListening();
  delayMicroseconds(128);
  radio.stopListening();
  
  curRSSI = radio.testRPD() ? random(-55, -45) : random(-90, -70);
  radio.powerDown();
  
  SPI.end();
  delay(10);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  channels[curCh] = curRSSI;
  if (curRSSI > peaks[curCh]) peaks[curCh] = curRSSI;
  
  curCh++;
  if (curCh > 125) {
    curCh = 0;
    for (int i = 0; i < 126; i++) {
      if (peaks[i] > -90) peaks[i] -= 2;
    }
  }
  
  // Малювання
  if (millis() - lastDraw >= 33) {
    lastDraw = millis();
    drawSpectrum();
  }
}

// =============================================
void initNRF() {
  SPI.end();
  delay(50);
  SPI.begin(TFT_SCLK, NRF_MISO, TFT_MOSI, NRF_CSN);
  
  nrfOK = radio.begin();
  if (nrfOK) {
    radio.setAutoAck(false);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.setPALevel(RF24_PA_MIN);
    radio.setChannel(0);
    radio.stopListening();
    nrfOK = radio.isChipConnected();
    radio.powerDown();
  }
  
  SPI.end();
  delay(50);
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  Serial.print("NRF24: ");
  Serial.println(nrfOK ? "OK" : "FAILED");
}

// =============================================
void showInitScreen() {
  tft.fillScreen(C_BG);
  
  tft.fillRect(0, 0, 160, 14, C_HEAD);
  tft.setTextColor(C_BG);
  tft.setCursor(5, 3);
  tft.print("NRF24 SCANNER");
  
  tft.setTextColor(C_OK);
  tft.setCursor(10, 25);
  tft.print("Display: OK");
  
  tft.setCursor(10, 40);
  if (nrfOK) {
    tft.setTextColor(C_OK);
    tft.print("NRF24: OK");
    tft.setTextColor(C_TEXT);
    tft.setCursor(10, 58);
    tft.print("Starting scan...");
  } else {
    tft.setTextColor(C_ERR);
    tft.print("NRF24: FAILED!");
    tft.setTextColor(C_WARN);
    tft.setCursor(10, 58);
    tft.print("CE=2 CSN=3 MISO=21");
  }
  
  tft.fillRect(0, 144, 160, 16, 0x1082);
  tft.setTextColor(0x8410);
  tft.setCursor(2, 148);
  tft.print(nrfOK ? "Ready..." : "Fix & reset");
}

// =============================================
void drawSpectrum() {
  // Заголовок
  tft.fillRect(0, 0, 160, 12, C_HEAD);
  tft.setTextColor(C_BG);
  tft.setCursor(3, 2);
  tft.print("CH:");
  tft.print(curCh);
  tft.setCursor(60, 2);
  tft.print("RSSI:");
  tft.print(curRSSI);
  
  // Графік
  int gy = 14;
  int gh = 58;
  tft.fillRect(0, gy, 160, gh, C_BG);
  
  // Сітка
  for (int y = gy; y < gy + gh; y += 14) {
    tft.drawFastHLine(0, y, 160, C_GRID);
  }
  
  // Стовпчики
  for (int i = 0; i < 126; i++) {
    int x = map(i, 0, 125, 2, 157);
    int h = map(channels[i], -90, -30, 0, gh);
    int y = gy + gh - h;
    
    uint16_t color = C_GRAPH;
    if (channels[i] > -50) color = C_WARN;
    if (channels[i] > -40) color = C_ERR;
    if (i == curCh) color = C_TEXT;
    
    if (h > 0) {
      tft.drawFastVLine(x, y, h, color);
    }
    
    // Пік
    if (peaks[i] > -90) {
      int ph = map(peaks[i], -90, -30, 0, gh);
      int py = gy + gh - ph;
      tft.drawPixel(x, py, C_PEAK);
    }
  }
  
  // Легенда
  tft.fillRect(0, 144, 160, 16, 0x1082);
  tft.setTextColor(0x8410);
  tft.setCursor(2, 148);
  tft.print("0");
  tft.setCursor(145, 148);
  tft.print("125");
}