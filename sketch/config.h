#ifndef CONFIG_H
#define CONFIG_H

// ===== ДИСПЛЕЙ ST7735 =====
#define TFT_CS    5
#define TFT_DC    6
#define TFT_RST   7
#define TFT_MOSI  10
#define TFT_SCK   8
#define TFT_LED   4   // підключати до 3.3V напряму, не через GPIO

// ===== PN532 I2C =====
#define PN532_SDA  20
#define PN532_SCL  21

// ===== КНОПКИ =====
#define BTN_SCAN     0   // GPIO0, INPUT_PULLUP, замикає на GND
#define BTN_EMULATE  1   // GPIO1, INPUT_PULLUP, замикає на GND

// ===== ПАРАМЕТРИ =====
#define MAX_UID_LEN   7   // максимальна довжина UID (Mifare = 4, 7 байт)
#define EMULATE_TIME  5000 // час емуляції в мс

#endif // CONFIG_H
