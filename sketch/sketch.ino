#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <RF24.h>

#define SCK_PIN   4
#define MISO_PIN  5
#define MOSI_PIN  6
#define CE_PIN    20
#define CSN_PIN   21

SPIClass spi(FSPI);
RF24 radio(CE_PIN, CSN_PIN);

void setupWiFi() {
  WiFiManager wm;
  bool connected = wm.autoConnect("ESP32-C3-Setup");
  if (!connected) {
    Serial.println("WiFi failed, restarting...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("WiFi connected! IP: " + WiFi.localIP().toString());
}

void setupOTA() {
  ArduinoOTA.setHostname("esp32c3-radio");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA Update started...");
    radio.stopConstCarrier();
    radio.stopListening();
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA Done! Rebooting...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]\n", error);
  });

  ArduinoOTA.begin();
  Serial.println("OTA ready!");
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  setupWiFi();
  setupOTA();

  spi.begin(SCK_PIN, MISO_PIN, MOSI_PIN);

  Serial.println("Initializing nRF24L01+...");
  if (!radio.begin(&spi)) {
    Serial.println("nRF24L01+ initialization failed!");
    while (1) {
      ArduinoOTA.handle();
      delay(100);
    }
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
  ArduinoOTA.handle();

  byte channel = random(0, 126);
  radio.setChannel(channel);
  delayMicroseconds(random(30, 100));
}

