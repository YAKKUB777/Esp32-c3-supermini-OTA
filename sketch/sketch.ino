/*
 * ESP32 OTA Programmer
 * ESP32-C3 Supermini -> звичайна ESP32
 * Веб-інтерфейс для завантаження прошивки
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <SPIFFS.h>

// Піни для керування цільовою ESP32
#define TARGET_TX     2
#define TARGET_RX     3
#define TARGET_BOOT   0
#define TARGET_EN     1

// Налаштування WiFi
const char* ssid = "OTA-Programmer";
const char* password = "12345678";

WebServer server(80);
IPAddress localIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// HTML сторінка
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 OTA Programmer</title>
    <style>
        body { background: #0a0a0a; color: #00ff00; font-family: monospace; padding: 20px; text-align: center; }
        h1 { color: #ff0000; }
        .card { background: #1a1a1a; border: 1px solid #00ff00; border-radius: 10px; padding: 20px; margin: 20px auto; max-width: 500px; }
        input[type="file"] { display: none; }
        .btn {
            background: #00ff00; color: #000; border: none; padding: 12px 24px;
            margin: 10px; border-radius: 5px; font-weight: bold; cursor: pointer; font-size: 14px;
        }
        .btn:hover { background: #00cc00; }
        .btn-attack { background: #ff8800; }
        .btn-attack:hover { background: #cc6600; }
        .progress-bar { width: 100%; height: 20px; background: #333; border-radius: 10px; overflow: hidden; margin: 20px 0; }
        .progress-fill { height: 100%; background: #00ff00; width: 0%; transition: width 0.3s; border-radius: 10px; }
        #status { color: #ffaa00; margin-top: 10px; font-size: 14px; }
        #info { color: #888; font-size: 12px; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="card">
        <h1>⚡ ESP32 OTA Programmer ⚡</h1>
        <p>ESP32-C3 → ESP32</p>
        
        <label for="firmware" class="btn">📁 Select Firmware (.bin)</label>
        <input type="file" id="firmware" accept=".bin" onchange="fileSelected()">
        
        <div id="filename" style="color:#00ff00; margin:10px;">No file selected</div>
        
        <button class="btn-attack" onclick="uploadFirmware()" id="uploadBtn" disabled>
            🚀 Upload & Flash
        </button>
        
        <div class="progress-bar">
            <div class="progress-fill" id="progress"></div>
        </div>
        
        <div id="status">Ready. Select firmware file.</div>
        <div id="info">Connect: WiFi "OTA-Programmer" | Password: 12345678</div>
    </div>

    <script>
        let selectedFile = null;
        
        function fileSelected() {
            selectedFile = document.getElementById('firmware').files[0];
            if (selectedFile) {
                document.getElementById('filename').textContent = selectedFile.name + ' (' + (selectedFile.size/1024).toFixed(2) + ' KB)';
                document.getElementById('uploadBtn').disabled = false;
            }
        }
        
        async function uploadFirmware() {
            if (!selectedFile) return;
            
            const btn = document.getElementById('uploadBtn');
            const progress = document.getElementById('progress');
            const status = document.getElementById('status');
            
            btn.disabled = true;
            status.textContent = 'Flashing... Please wait! Do NOT disconnect!';
            
            const formData = new FormData();
            formData.append('firmware', selectedFile);
            
            try {
                const xhr = new XMLHttpRequest();
                xhr.open('POST', '/update', true);
                
                xhr.upload.onprogress = (e) => {
                    if (e.lengthComputable) {
                        const percent = Math.round((e.loaded / e.total) * 100);
                        progress.style.width = percent + '%';
                        status.textContent = 'Uploading: ' + percent + '%';
                    }
                };
                
                xhr.onload = () => {
                    if (xhr.status === 200) {
                        progress.style.width = '100%';
                        status.textContent = '✅ FLASHING COMPLETE! Target ESP32 is running new firmware!';
                        status.style.color = '#00ff00';
                    } else {
                        status.textContent = '❌ Error: ' + xhr.statusText;
                        status.style.color = '#ff0000';
                    }
                    btn.disabled = false;
                };
                
                xhr.onerror = () => {
                    status.textContent = '❌ Connection error!';
                    status.style.color = '#ff0000';
                    btn.disabled = false;
                };
                
                xhr.send(formData);
                
            } catch(e) {
                status.textContent = '❌ Error: ' + e.message;
                status.style.color = '#ff0000';
                btn.disabled = false;
            }
        }
    </script>
</body>
</html>
)rawliteral";

// =============================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 OTA PROGRAMMER ===");

  // Налаштування пінів для цільової ESP32
  pinMode(TARGET_BOOT, OUTPUT);
  pinMode(TARGET_EN, OUTPUT);
  
  // Спочатку вимикаємо цільову ESP32
  digitalWrite(TARGET_EN, LOW);
  digitalWrite(TARGET_BOOT, HIGH);
  
  // UART для зв'язку з цільовою ESP32
  Serial1.begin(115200, SERIAL_8N1, TARGET_RX, TARGET_TX);

  // WiFi AP
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(localIP, gateway, subnet);
  
  Serial.print("WiFi: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Password: ");
  Serial.println(password);

  // Веб-сервер
  server.on("/", []() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/update", HTTP_POST, []() {
    server.send(200, "text/plain", "OK");
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Flashing: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
      
      // Переводимо цільову ESP32 в режим прошивки
      digitalWrite(TARGET_EN, LOW);
      delay(100);
      digitalWrite(TARGET_BOOT, LOW);  // BOOT = LOW для режиму прошивки
      delay(100);
      digitalWrite(TARGET_EN, HIGH);   // Вмикаємо ESP32
      delay(500);
      
      // Запускаємо прошивку через Serial1
      Serial1.printf("## FLASH START: %u\n", upload.totalSize);
      delay(100);
    }
    
    if (upload.status == UPLOAD_FILE_WRITE) {
      // Відправляємо дані через UART на цільову ESP32
      Serial1.write(upload.buf, upload.currentSize);
      Serial.printf("Sent %u bytes\n", upload.currentSize);
    }
    
    if (upload.status == UPLOAD_FILE_END) {
      Serial.println("Flashing complete! Rebooting target...");
      Serial1.printf("## FLASH END\n");
      
      // Перезавантажуємо цільову ESP32
      delay(500);
      digitalWrite(TARGET_EN, LOW);
      delay(100);
      digitalWrite(TARGET_BOOT, HIGH);  // BOOT = HIGH для нормальної роботи
      delay(100);
      digitalWrite(TARGET_EN, HIGH);
    }
  });

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Connect to WiFi and open 192.168.4.1");
}

void loop() {
  server.handleClient();
  delay(2);
}