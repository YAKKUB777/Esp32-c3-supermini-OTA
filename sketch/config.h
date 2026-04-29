#ifndef CONFIG_H
#define CONFIG_H

// ===== Піни дисплея ST7735 0.96" RGB 160x80 =====
#define TFT_CS     5
#define TFT_DC     6
#define TFT_RST    7
#define TFT_SCLK   8
#define TFT_MOSI   10

// ===== Піни NRF24L01 =====
#define NRF_CE     2
#define NRF_CSN    3
#define NRF_MISO   21

// ===== Кольори =====
#define C_BG        ST77XX_BLACK
#define C_HEAD      ST77XX_CYAN
#define C_OK        ST77XX_GREEN
#define C_ERR       ST77XX_RED
#define C_WARN      ST77XX_YELLOW
#define C_TEXT      ST77XX_WHITE
#define C_GRAPH     ST77XX_GREEN
#define C_PEAK      ST77XX_RED
#define C_GRID      0x4208

// ===== Налаштування NRF24 =====
#define SCAN_MIN_CH     0
#define SCAN_MAX_CH     125
#define SCAN_DELAY_US   128

#endif