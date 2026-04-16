#include <SPI.h>
#include <RF24.h>

#define CE_PIN   8
#define CSN_PIN  7

RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  radio.begin();
  radio.setAddressWidth(3); 
  radio.setPALevel(RF24_PA_MAX);      [span_6](start_span)// Максимум 11dBm[span_6](end_span)
  radio.setDataRate(RF24_2MBPS);     // Максимальна ширина спектра одного пакета
  radio.setAutoAck(false);           // Вимикаємо перевірку, щоб не чекати відповіді
  radio.stopListening();
}

void loop() {
  // Швидко проходимо по основних каналах Wi-Fi (1, 6, 11) 
  [span_7](start_span)// або по всьому діапазону 0-125[span_7](end_span)
  for (int i = 0; i < 126; i++) {
    radio.setChannel(i);
    
    // Відправляємо "сміттєвий" пакет без очікування
    const uint8_t dummy[] = {0xAA, 0xAA, 0xAA, 0xAA};
    radio.startWrite(&dummy, sizeof(dummy), true); 
    
    // Мінімальна затримка для стабілізації синтезатора частоти
    delayMicroseconds(10); 
  }
}
