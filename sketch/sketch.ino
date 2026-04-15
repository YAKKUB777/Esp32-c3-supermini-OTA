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

// ====================== Вершини куба (8 точок) ======================
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

// ====================== Кольори граней (RGB565) ======================
uint16_t faceColors[6] = {
  ST77XX_RED,     // задня
  ST77XX_GREEN,   // права
  ST77XX_BLUE,    // передня
  ST77XX_YELLOW,  // ліва
  ST77XX_CYAN,    // верхня
  ST77XX_MAGENTA  // нижня
};

// Грані (4 вершини на грань)
int faces[6][4] = {
  {0,1,2,3}, // задня
  {1,5,6,2}, // права
  {5,4,7,6}, // передня
  {4,0,3,7}, // ліва
  {3,2,6,7}, // верхня
  {4,5,1,0}  // нижня
};

// ====================== Змінні для обертання ======================
float angleX = 0, angleY = 0, angleZ = 0;
float rotSpeedX = 0.02, rotSpeedY = 0.03, rotSpeedZ = 0.01;

// ====================== Функція проекції 3D -> 2D ======================
void project(float x, float y, float z, int *x2d, int *y2d) {
  float scale = 70 / (z + 2.5);
  *x2d = 64 + (int)(x * scale * 30);
  *y2d = 80 + (int)(y * scale * 30);
}

// ====================== Функція повороту точки ======================
void rotate(float *x, float *y, float *z) {
  float x1, y1, z1;
  
  // Поворот навколо X
  y1 = *y * cos(angleX) - *z * sin(angleX);
  z1 = *y * sin(angleX) + *z * cos(angleX);
  *y = y1;
  *z = z1;
  
  // Поворот навколо Y
  x1 = *x * cos(angleY) + *z * sin(angleY);
  z1 = -*x * sin(angleY) + *z * cos(angleY);
  *x = x1;
  *z = z1;
  
  // Поворот навколо Z
  x1 = *x * cos(angleZ) - *y * sin(angleZ);
  y1 = *x * sin(angleZ) + *y * cos(angleZ);
  *x = x1;
  *y = y1;
}

// ====================== Обчислення нормалі грані ======================
void getFaceNormal(int faceIndex, float *nx, float *ny, float *nz) {
  int *v = faces[faceIndex];
  float v1x = cubeVertices[v[1]][0] - cubeVertices[v[0]][0];
  float v1y = cubeVertices[v[1]][1] - cubeVertices[v[0]][1];
  float v1z = cubeVertices[v[1]][2] - cubeVertices[v[0]][2];
  
  float v2x = cubeVertices[v[3]][0] - cubeVertices[v[0]][0];
  float v2y = cubeVertices[v[3]][1] - cubeVertices[v[0]][1];
  float v2z = cubeVertices[v[3]][2] - cubeVertices[v[0]][2];
  
  *nx = v1y * v2z - v1z * v2y;
  *ny = v1z * v2x - v1x * v2z;
  *nz = v1x * v2y - v1y * v2x;
}

// ====================== Малювання куба ======================
void drawCube() {
  int points2d[8][2];
  float rotatedVertices[8][3];
  
  // Копіюємо та повертаємо вершини
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
  
  // Малюємо грані (від далеких до ближніх)
  for (int i = 0; i < 6; i++) {
    float nx, ny, nz;
    getFaceNormal(i, &nx, &ny, &nz);
    
    // Повертаємо нормаль
    float rx = nx, ry = ny, rz = nz;
    rotate(&rx, &ry, &rz);
    
    // Якщо грань видима (направлена до камери)
    if (rz < 0) {
      int x1 = points2d[faces[i][0]][0];
      int y1 = points2d[faces[i][0]][1];
      int x2 = points2d[faces[i][1]][0];
      int y2 = points2d[faces[i][1]][1];
      int x3 = points2d[faces[i][2]][0];
      int y3 = points2d[faces[i][2]][1];
      int x4 = points2d[faces[i][3]][0];
      int y4 = points2d[faces[i][3]][1];
      
      tft.fillTriangle(x1, y1, x2, y2, x3, y3, faceColors[i]);
      tft.fillTriangle(x1, y1, x3, y3, x4, y4, faceColors[i]);
    }
  }
  
  // Малюємо ребра чорним кольором для контрасту
  for (int i = 0; i < 12; i++) {
    int p1 = edges[i][0];
    int p2 = edges[i][1];
    tft.drawLine(points2d[p1][0], points2d[p1][1],
                 points2d[p2][0], points2d[p2][1],
                 ST77XX_BLACK);
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
  // Оновлюємо кути обертання
  angleX += rotSpeedX;
  angleY += rotSpeedY;
  angleZ += rotSpeedZ;
  
  drawCube();
  delay(30);
}