#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Піни для ESP32-C3 SuperMini
#define SCK_PIN   4
#define MISO_PIN  5
#define MOSI_PIN  6
#define CE_PIN    20
#define CSN_PIN   21

SPIClass spi(FSPI);  
RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  Serial.begin(115200);
  delay(500);  

  // Ініціалізація апаратної шини SPI
  spi.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  Serial.println("Initializing nRF24L01+ PA+LNA...");

  if (!radio.begin(&spi)) {
    Serial.println("nRF24L01+ initialization failed!");
    while (1);
  }

  // Налаштування агресивного джаммінгу
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);  
  radio.setCRCLength(RF24_CRC_DISABLED);  
  radio.stopListening();
  
  // Початкова активація носія
  radio.startConstCarrier(RF24_PA_MAX, 45);  

  Serial.println("System Ready. Start Random Hopping.");
}

void loop() {
  // Справжній рандом на базі апаратного RNG ESP32
  uint8_t channel = esp_random() % 126;
  
  radio.setChannel(channel);
  
  // В деяких версіях бібліотеки зміна каналу скидає ConstCarrier, 
  // тому іноді корисно викликати його повторно:
  radio.startConstCarrier(RF24_PA_MAX, channel);

  // Рандомна затримка для непередбачуваності
  delayMicroseconds(random(30, 100));  
}
