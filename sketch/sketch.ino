#include <SPI.h>
#include <RF24.h>

// Піни для ESP32-C3 SuperMini
#define CE_PIN   8
#define CSN_PIN  7

RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  // Ініціалізація радіо
  if (!radio.begin()) {
    return;
  }
  
  // Базові налаштування потужності та швидкості
  radio.setPALevel(RF24_PA_MAX); // Максимальна потужність (11dBm для E01C)
  radio.setDataRate(RF24_2MBPS); 
  radio.setAutoAck(false);       
  radio.setRetries(0, 0);
  radio.stopListening();

  // Активація режиму постійної несучої через стандартну функцію бібліотеки.
  // Це безпечно змінить біт CONT_WAVE у регістрі 0x06 (RF_SETUP).
  // Параметри: рівень потужності та канал (для ініціалізації)
  radio.startConstCarrier(RF24_PA_MAX, 0);
}

void loop() {
  // Цикл швидкого проходження частот (Frequency Hopping Jammer)
  // Ми використовуємо setChannel всередині циклу для "прошивання" ефіру
  for (int i = 0; i < 126; i++) {
    radio.setChannel(i);
    
    // В режимі ConstCarrier зміна каналу автоматично переносить несучу.
    // Відправка пакету додає додаткову модуляцію шуму.
    const uint8_t jam_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
    radio.startWrite(&jam_data, sizeof(jam_data), true);
    
    delayMicroseconds(50); 
  }
}
