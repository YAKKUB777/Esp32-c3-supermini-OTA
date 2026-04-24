#ifndef CONFIG_H
#define CONFIG_H

// ====================== Піни для ESP32-C3 Supermini ======================
// Дисплей ST7735 (0.96", 80x160)
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8
#define TFT_LED     20      // підсвітка (PWM)

// PN532 (I2C)
#define PN532_SDA   21
#define PN532_SCL   22

// Кнопки (опціонально)
#define BTN_A       0       // GPIO0 (BOOT кнопка)
#define BTN_B       1       // GPIO1

// ====================== Налаштування ======================
#define EMULATION_TIMEOUT_MS  5000   // таймаут емуляції (мс)
#define DISPLAY_TIMEOUT_MS    30000  // таймаут вимкнення дисплея (мс)

#endif