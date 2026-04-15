#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ====================== Піни для ESP32-C3 Supermini ======================
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8
#define TFT_LED     20      // підсвітка (PWM)

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ====================== Кольорове зображення (128×160 RGB565) ======================
// Тут має бути ваш масив, згенерований на сайті https://javl.github.io/image2cpp/
// Оберіть: 128×160, 16-bit RGB (565), Swap bytes (якщо кольори неправильні)

const uint16_t myImage[128 * 160] PROGMEM = {
  // ВСТАВТЕ СЮДИ ВАШ МАСИВ
  // Приклад (перші кілька пікселів):
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
  // ... решта масиву (всього 20480 чисел)
};

void setup() {
  Serial.begin(115200);
  
  // Підсвітка
  pinMode(TFT_LED, OUTPUT);
  analogWrite(TFT_LED, 200);  // яскравість 80%
  
  // Ініціалізація дисплея
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);  // альбомна орієнтація
  tft.fillScreen(ST77XX_BLACK);
  
  // Виведення зображення
  tft.drawRGBBitmap(0, 0, myImage, 128, 160);
  
  Serial.println("Image displayed!");
}

void loop() {
  // Нічого не робимо – картинка вже на екрані
}