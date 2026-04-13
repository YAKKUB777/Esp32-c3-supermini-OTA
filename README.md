📡 ESP32-C3 2.4GHz Jammer
⚠️ Disclaimer: This project is for educational and research purposes only.
Jamming radio frequencies may be illegal in your country. Use only in isolated environments or with proper authorization.
🔥 About
A compact and powerful 2.4GHz band jammer built on the ESP32-C3 SuperMini with an nRF24L01+ radio module.
Uses a three-stage jamming strategy to effectively disrupt WiFi, Bluetooth, BLE, and Zigbee simultaneously. No display, no WiFi, no OTA — just pure jamming from the moment it powers on.
✨ Features
📶 Three-stage jamming strategy:
WiFi channels (1–13) with priority on 1, 6, 11
BLE advertising channels (37, 38, 39) — hit 3x per cycle
Full sweep of all 126 channels for Zigbee and others
⚡ Maximum PA level (RF24_PA_MAX) and 2Mbps data rate
🚀 Starts jamming instantly on power-on — no setup needed
🔋 Runs on any USB power bank or LiPo battery
🔧 Firmware compiled automatically via GitHub Actions
🛒 Hardware
Component
Details
Microcontroller  ESP32-C3 SuperMini 
Radio module nRF24L01+ 
Power  USB-C or LiPo 3.7V  
🔌 Wiring
nRF24L01+ESP32-C3 SuperMini

VCC  3.3V 
GND  GND  
CE   GPIO 20 
CSN  GPIO 21 
SCK  GPIO 4 
MOSI GPIO 6 
MISO GPIO 5 
IRQ   — 

📡 How It Works
The nRF24L01+ is configured in startConstCarrier mode — it transmits a raw unmodulated signal at maximum power. Combined with rapid channel hopping across the entire 2.4GHz spectrum, this saturates the band and disrupts all nearby 2.4GHz communication.
Jamming cycle per loop:
1. WiFi  → channels 1,6,11,2,7,12... (150µs each)
2. BLE   → channels 37,38,39 × 3    (300µs each)
3. Sweep → one channel per iteration (100µs)
🚀 Flashing
Get the firmware
Download the latest firmware-jammer-c3 from Actions → Artifacts and extract merged.bin.
Flash via browser (no PC needed)
Open esp.huhn.me in Chrome
Connect ESP32-C3 via USB
Select merged.bin → offset 0x0 → Erase → Flash
🏗️ Build System
Every push to main triggers a GitHub Actions build that:
Installs arduino-cli and ESP32 platform
Compiles for esp32:esp32:esp32c3
Saves firmware as a downloadable artifact
📄 License
MIT License — do whatever you want, but stay legal.
�
Made with ❤️ and a phone

