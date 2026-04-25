#ifndef CONFIG_H
#define CONFIG_H

// ====================== Піни для ESP32-C3 Supermini ======================
// Дисплей ST7735 (0.96", 80x160)
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8
#define TFT_LED     4       // ← змінено з 20 на 4

// PN532 (I2C) – тільки GPIO20 та GPIO21 працюють для I2C
#define PN532_SDA   20
#define PN532_SCL   21

// Кнопки
#define BTN_SCAN    0       // GPIO0 (BOOT)
#define BTN_EMULATE 1

#endif