#include <SPI.h>
#include "RF24.h"

#define SCK_PIN   4
#define MISO_PIN  5
#define MOSI_PIN  6
#define CE_PIN    20
#define CSN_PIN   21

SPIClass spi(FSPI);
RF24 radio(CE_PIN, CSN_PIN);

// WiFi канали 1-13 (центральні частоти)
const byte wifi_ch[] = {1, 6, 11, 2, 7, 12, 3, 8, 13, 4, 9, 5, 10};
const byte wifi_count = 13;

// BLE advertising канали
const byte ble_ch[] = {37, 38, 39};

// Всі канали по порядку для загального sweep
byte sweep_index = 0;

void setup() {
  spi.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  radio.begin(&spi);
  radio.setAutoAck(false);
  radio.stopListening();
  radio.setRetries(0, 0);
  radio.setPayloadSize(5);
  radio.setAddressWidth(3);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.startConstCarrier(RF24_PA_MAX, 40);
}

void loop() {
  // 1. Пройтись по WiFi каналах (найбільш вживані)
  for (int i = 0; i < wifi_count; i++) {
    radio.setChannel(wifi_ch[i]);
    delayMicroseconds(150);
  }

  // 2. BLE advertising — по 3 рази кожен
  for (int j = 0; j < 3; j++) {
    radio.setChannel(ble_ch[0]); delayMicroseconds(300);
    radio.setChannel(ble_ch[1]); delayMicroseconds(300);
    radio.setChannel(ble_ch[2]); delayMicroseconds(300);
  }

  // 3. Загальний sweep решти каналів
  radio.setChannel(sweep_index);
  sweep_index = (sweep_index + 1) % 126;
  delayMicroseconds(100);
}