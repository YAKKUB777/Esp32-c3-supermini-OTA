#include <SPI.h>
#include <RF24.h>
#include <nRF24L01.h>

// Піни для ESP32-C3 SuperMini
#define CE_PIN   8
#define CSN_PIN  7

RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  // Ініціалізація радіо
  if (!radio.begin()) {
    return;
  }
  
  // Налаштування для модуля E01C-2G4M11S (Ci24R1)
  radio.setPALevel(RF24_PA_MAX); // Максимальна потужність 11dBm
  radio.setDataRate(RF24_2MBPS); // Ширша смуга завад
  radio.setAutoAck(false);       // Вимикаємо підтвердження
  radio.setRetries(0, 0);
  radio.stopListening();

  // Прямий запис у регістр для активації постійного випромінювання (CONT_WAVE)
  // Використовуємо явне вказання простору імен для запобігання помилок scope
  uint8_t setup_reg = radio.read_register(0x06); // 0x06 - це RF_SETUP
  radio.write_register(0x06, setup_reg | 0x80); 
}

void loop() {
  // Цикл швидкого проходження частот для створення "стіни шуму"
  for (int i = 0; i < 126; i++) {
    radio.setChannel(i);
    
    // Відправка порожнього пакета
    const uint8_t jam_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    radio.startWrite(&jam_data, sizeof(jam_data), true);
    
    // Мінімальна пауза для стабілізації частоти
    delayMicroseconds(50); 
  }
}
