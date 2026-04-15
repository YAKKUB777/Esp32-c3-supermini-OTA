#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ====================== Піни для ESP32-C3 Supermini ======================
#define TFT_CS      5
#define TFT_DC      6
#define TFT_RST     7
#define TFT_MOSI    10
#define TFT_SCLK    8
#define TFT_LED     20

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ====================== Вершини куба ======================
float cubeVertices[8][3] = {
  {-1, -1, -1}, // 0
  { 1, -1, -1}, // 1
  { 1,  1, -1}, // 2
  {-1,  1, -1}, // 3
  {-1, -1,  1}, // 4
  { 1, -1,  1}, // 5
  { 1,  1,  1}, // 6
  {-1,  1,  1}  // 7
};

// ====================== Ребра куба (12 з'єднань) ======================
int edges[12][2] = {
  {0,1}, {1,2}, {2,3}, {3,0}, // задня грань
  {4,5}, {5,6}, {6,7}, {7,4}, // передня грань
  {0,4}, {1,5}, {2,6}, {3,7}  // з'єднання
};

// ====================== Змінні для обертання ======================
float angleX = 0, angleY = 0, angleZ = 0;
float rotSpeedX = 0.02, rotSpeedY = 0.03, rotSpeedZ = 0.01;

// ====================== Проекція 3D -> 2D ======================
void project(float x, float y, float z, int *x2d, int *y2d) {
  float scale = 65 / (z + 2.5);
  *x2d = 64 + (int)(x * scale * 32);
  *y2d = 80 + (int)(y * scale * 32);
}

// ====================== Поворот точки ======================
void rotate(float *x, float *y, float *z) {
  float x1, y1, z1;
  
  y1 = *y * cos(angleX) - *z * sin(angleX);
  z1 = *y * sin(angleX) + *z * cos(angleX);
  *y = y1;
  *z = z1;
  
  x1 = *x * cos(angleY) + *z * sin(angleY);
  z1 = -*x * sin(angleY) + *z * cos(angleY);
  *x = x1;
  *z = z1;
  
  x1 = *x * cos(angleZ) - *y * sin(angleZ);
  y1 = *x * sin(angleZ) + *y * cos(angleZ);
  *x = x1;
  *y = y1;
}

// ====================== Малювання куба (тільки ребра) ======================
void drawCube() {
  int points2d[8][2];
  float rotatedVertices[8][3];
  
  // Поворот і проекція вершин
  for (int i = 0; i < 8; i++) {
    rotatedVertices[i][0] = cubeVertices[i][0];
    rotatedVertices[i][1] = cubeVertices[i][1];
    rotatedVertices[i][2] = cubeVertices[i][2];
    rotate(&rotatedVertices[i][0], &rotatedVertices[i][1], &rotatedVertices[i][2]);
    project(rotatedVertices[i][0], rotatedVertices[i][1], rotatedVertices[i][2], 
            &points2d[i][0], &points2d[i][1]);
    
    // Обмеження координат
    points2d[i][0] = constrain(points2d[i][0], 2, 126);
    points2d[i][1] = constrain(points2d[i][1], 2, 158);
  }
  
  // Малюємо ребра білим кольором
  for (int i = 0; i < 12; i++) {
    int p1 = edges[i][0];
    int p2 = edges[i][1];
    tft.drawLine(points2d[p1][0], points2d[p1][1],
                 points2d[p2][0], points2d[p2][1],
                 ST77XX_WHITE);
  }
  
  // Малюємо вершини червоними точками (опціонально)
  for (int i = 0; i < 8; i++) {
    tft.fillCircle(points2d[i][0], points2d[i][1], 2, ST77XX_RED);
  }
}

void setup() {
  // Підсвітка
  pinMode(TFT_LED, OUTPUT);
  analogWrite(TFT_LED, 180);
  
  // Ініціалізація дисплея
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
}

void loop() {
  tft.fillScreen(ST77XX_BLACK);
  
  angleX += rotSpeedX;
  angleY += rotSpeedY;
  angleZ += rotSpeedZ;
  
  drawCube();
  delay(30);
}