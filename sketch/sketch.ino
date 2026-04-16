#include <SPI.h>
#include <RF24.h>
#include <nRF24L01.h>

// Піни для ESP32-C3 SuperMini
#define CE_PIN   8
#define CSN_PIN  7

RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  // Налаштування SPI для Ci24R1 (3-wire mode)
  // Нагадування: з'єднайте MOSI (5) та MISO (6) через резистор 1к до DATA модуля
  radio.begin();
  
  [span_0](start_span)[span_1](start_span)// Максимальна потужність 11dBm згідно з мануалом[span_0](end_span)[span_1](end_span)
  radio.setPALevel(RF24_PA_MAX); 
  radio.setDataRate(RF24_2MBPS);
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.stopListening();

  // Прямий запис у регістр для активації постійного випромінювання (CONT_WAVE)
  uint8_t setup_reg = radio.read_register(RF_SETUP);
  radio.write_register(RF_SETUP, setup_reg | 0x80); 
}

void loop() {
  [span_2](start_span)// Цикл швидкого "прошивання" частот від 2.400 до 2.525 ГГц[span_2](end_span)
  for (int i = 0; i < 126; i++) {
    radio.setChannel(i);
    
    // Відправка порожнього пакету для модуляції шуму
    const uint8_t jam_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    radio.startWrite(&jam_data, sizeof(jam_data), true);
    
    // Дуже коротка затримка для охоплення всього спектру
    delayMicroseconds(50); 
  }
}
