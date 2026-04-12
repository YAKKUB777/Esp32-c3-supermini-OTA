# Esp32-c3-supermini-OTA

📡 ESP32-C3 2.4GHz Jammer with OTA Updates

⚠️ Disclaimer: This project is intended for educational and research purposes only.
Jamming radio frequencies may be illegal in your country. Use only in isolated environments or with proper authorization.

🔥 About
This is my first serious GitHub project — a 2.4GHz band jammer built on the ESP32-C3 SuperMini with an nRF24L01+ radio module.
The device continuously hops across all 2.4GHz channels (0–125), broadcasting a constant carrier signal to saturate the band. This affects WiFi, Bluetooth, and other 2.4GHz devices in range.
What makes this build stand out — no PC required after the first flash. All future firmware updates are delivered Over-The-Air (OTA) via WiFi, directly from your phone or any device on the same network.

✨ Features

📶 Constant carrier signal across 2.4GHz band

🔀 Random channel hopping (0–126)

📡 Maximum PA level (RF24_PA_MAX)

🚀 OTA firmware updates — no USB needed after first flash

📱 WiFiManager — configure WiFi from your phone, no hardcoded passwords

🔧 Compiled automatically via GitHub Actions — no Arduino IDE needed

💾 Firmware .bin artifact ready to download after every commit

🛒 Hardware
Component
Details
Microcontroller
ESP32-C3 SuperMini
Radio module
nRF24L01+
Power
USB-C or 3.3V
Wiring
nRF24L01+
ESP32-C3 SuperMini
SCK → GPIO 4  
MISO → GPIO 5   
MOSI → GPIO 6   
CE → GPIO 20  
CSN → GPIO 21  
VCC → 3.3V
GND → GND

🚀 Getting Started
First Flash (USB required)
Download the latest firmware-esp32c3 from Actions → Artifacts
Extract the .bin file
Open esp.huhn.me in Chrome on your phone or PC
Connect ESP32-C3 via USB → select the .bin → flash
WiFi Setup (first boot)
Power on the device
Connect to the WiFi hotspot "ESP32-C3-Setup" from your phone
Enter your home WiFi credentials in the browser page that opens
Device saves credentials and connects automatically from now on
OTA Updates (all future updates)
Edit code → push to main branch
GitHub Actions compiles automatically (~5 min)
Download new .bin from Actions → Artifacts
Flash wirelessly via esp.huhn.me or Arduino IDE OTA port

⚙️ How It Works
Setup → Connect WiFi (WiFiManager) → Init OTA → Init nRF24L01+
Loop  → OTA.handle() → random channel (0-126) → setChannel() → delay
The nRF24L01+ is set to startConstCarrier mode at maximum power, which transmits a raw unmodulated signal. Combined with rapid channel hopping, this saturates the entire 2.4GHz spectrum.

🏗️ Build System
This project uses GitHub Actions to compile firmware automatically — no Arduino IDE or PC needed.
Every push to main triggers a build that:
Installs arduino-cli
Installs ESP32 board support
Installs RF24 and WiFiManager libraries
Compiles for esp32:esp32:esp32c3
Saves .bin as a downloadable artifact

📄 License
MIT License — do whatever you want, but stay legal.
�
Made with ❤️ and a phone
