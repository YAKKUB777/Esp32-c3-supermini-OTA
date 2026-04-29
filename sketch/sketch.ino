#include <Arduino.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>


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

  spi.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  Serial.println("Initializing nRF24L01+...");

  if (!radio.begin(&spi)) {
    Serial.println("nRF24L01+ initialization failed!");
    while (1);
  }

  Serial.println("nRF24L01+ initialized!");


  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);  
  radio.setCRCLength(RF24_CRC_DISABLED);  
  radio.setChannel(0);  
  radio.stopListening();
  radio.startConstCarrier(RF24_PA_MAX, 45);  


  randomSeed(analogRead(0));  
}

void loop() {

  byte channel = random(0, 126);
  radio.setChannel(channel);
  delayMicroseconds(random(30, 100));  
}