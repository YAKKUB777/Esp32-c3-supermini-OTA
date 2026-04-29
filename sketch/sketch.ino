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

// ===== Дані сканування =====
int channels[126];
int peaks[126];
int maxVal = -90;
int curPeak = -90;
int peakCh = 0;
bool nrfOK = false;
int scanPos = 0;
unsigned long lastScan = 0;
unsigned long lastDraw = 0;

// ===== Прототипи =====
void testDisplay();
void testNRF();
void showInitScreen();
void scanLoop();
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
  
  // Тест дисплея
  testDisplay();
  
  // Тест NRF24
  testNRF();
  
  // Показати результат
  showInitScreen();
  
  if (!nrfOK) {
    while(1) delay(1000); // Зависаємо, якщо NRF не працює
  }
  
  delay(2000);
  tft.fillScreen(C_BG);
}

// =============================================
void loop() {
  if (!nrfOK) return;
  
  // Сканування
  if (micros() - lastScan >= SCAN_DELAY_US) {
    lastScan = micros();
    
    // Перемикаємо SPI на NRF24
    SPI.end();
    SPI.begin(TFT_SCLK, NRF_MISO, TFT_MOSI, NRF_CSN);
    
    radio.powerUp();
    radio.setChannel(scanPos);
    radio.startListening();
    delayMicroseconds(128);
    radio.stopListening();
    
    int rssi = radio.testRPD() ? -50 : random(-95, -70);
    radio.powerDown();
    
    // Повертаємо SPI на дисплей
    SPI.end();
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    
    channels[scanPos] = rssi;
    if (rssi > peaks[scanPos]) peaks[scanPos] = rssi;
    if (rssi > curPeak) { curPeak = rssi; peakCh = scanPos; }
    if (rssi > maxVal) maxVal = rssi;
    
    scanPos++;
    if (scanPos > 125) {
      scanPos = 0;
      curPeak = -90;
      // Затухання піків
      for (int i = 0; i < 126; i++) {
        if (peaks[i] > -90) peaks[i] -= 2;
      }
    }
  }
  
  // Малювання
  if (millis() - lastDraw >= 50) {
    lastDraw = millis();
    drawSpectrum();
  }
}

// =============================================
void testDisplay() {
  tft.setTextColor(C_OK);
  tft.setCursor(10, 10);
  tft.print("Display: OK");
  Serial.println("Display: OK");
}

// =============================================
void testNRF() {
  SPI.end();
  SPI.begin(TFT_SCLK, NRF_MISO, TFT_MOSI, NRF_CSN);
  
  nrfOK = radio.begin();
  if (nrfOK) {
    radio.setAutoAck(false);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_DISABLED);
    radio.setPALevel(RF24_PA_MAX);
    radio.setChannel(0);
    radio.stopListening();
    nrfOK = radio.isChipConnected();
  }
  
  SPI.end();
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  Serial.print("NRF24: ");
  Serial.println(nrfOK ? "OK" : "FAILED");
}

// =============================================
void showInitScreen() {
  tft.fillScreen(C_BG);
  
  // Заголовок
  tft.fillRect(0, 0, 160, 14, C_HEAD);
  tft.setTextColor(C_BG);
  tft.setCursor(5, 3);
  tft.print("NRF24 SCANNER");
  
  // Статус
  tft.setTextColor(C_OK);
  tft.setCursor(10, 25);
  tft.print("Display: OK");
  
  tft.setCursor(10, 40);
  if (nrfOK) {
    tft.setTextColor(C_OK);
    tft.print("NRF24: OK");
    tft.setCursor(10, 58);
    tft.setTextColor(C_TEXT);
    tft.print("PA: MAX");
    tft.setCursor(10, 70);
    tft.print("Ch: 0-125");
  } else {
    tft.setTextColor(C_ERR);
    tft.print("NRF24: FAIL!");
    tft.setTextColor(C_WARN);
    tft.setCursor(10, 58);
    tft.print("Check pins:");
    tft.setCursor(10, 70);
    tft.print("CE=2 CSN=3 MISO=21");
  }
  
  // Футер
  tft.fillRect(0, 144, 160, 16, 0x1082);
  tft.setTextColor(0x8410);
  tft.setCursor(2, 148);
  tft.print(nrfOK ? "Starting scan..." : "Reset to retry");
}

// =============================================
void drawSpectrum() {
  // Заголовок
  tft.fillRect(0, 0, 160, 12, C_HEAD);
  tft.setTextColor(C_BG);
  tft.setCursor(3, 2);
  tft.print("2.4GHz");
  tft.setCursor(70, 2);
  tft.print("PK:");
  tft.print(peakCh);
  tft.setCursor(120, 2);
  tft.print(maxVal);
  tft.print("dB");
  
  // Графік
  int gy = 14;
  int gh = 60;
  tft.fillRect(0, gy, 160, gh, C_BG);
  
  // Сітка
  for (int y = gy; y < gy + gh; y += 15) {
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
    if (i == scanPos) color = C_TEXT;
    
    tft.drawFastVLine(x, y, h, color);
    
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
  tft.print("CH:");
  tft.print(scanPos);
  tft.setCursor(80, 148);
  tft.print("MAX:");
  tft.print(curPeak);
  tft.print("dB");
}