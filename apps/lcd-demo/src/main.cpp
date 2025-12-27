// ESP32-S3-LCD-1.47B - Tilt Navigation Demo (Landscape)
// Gesture: Tilt back + return to level = next, Tilt forward + return = previous

#include <Arduino.h>
#include <Wire.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ============== Display Configuration ==============
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void) {
    auto cfg = _bus_instance.config();
    cfg.spi_host = SPI2_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 80000000;
    cfg.freq_read = 16000000;
    cfg.pin_sclk = 40;
    cfg.pin_mosi = 45;
    cfg.pin_miso = -1;
    cfg.pin_dc = 41;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);

    auto cfg_panel = _panel_instance.config();
    cfg_panel.pin_cs = 42;
    cfg_panel.pin_rst = 39;
    cfg_panel.pin_busy = -1;
    cfg_panel.memory_width = 172;
    cfg_panel.memory_height = 320;
    cfg_panel.panel_width = 172;
    cfg_panel.panel_height = 320;
    cfg_panel.offset_x = 34;
    cfg_panel.offset_y = 0;
    cfg_panel.invert = true;
    cfg_panel.rgb_order = false;
    _panel_instance.config(cfg_panel);

    auto cfg_light = _light_instance.config();
    cfg_light.pin_bl = 46;
    cfg_light.invert = false;
    cfg_light.freq = 44100;
    cfg_light.pwm_channel = 7;
    _light_instance.config(cfg_light);
    _panel_instance.setLight(&_light_instance);

    setPanel(&_panel_instance);
  }
};

LGFX lcd;
LGFX_Sprite sprite(&lcd);  // Sprite buffer for flicker-free drawing

// Landscape dimensions
const int SCREEN_WIDTH = 320;
const int SCREEN_HEIGHT = 172;

// ============== IMU Configuration ==============
#define I2C_SDA 48
#define I2C_SCL 47
#define QMI8658_ADDR 0x6B

float accelX = 0, accelY = 0, accelZ = 0;
const float ACCEL_SCALE = 4.0 / 32768.0;

// ============== Gesture Detection ==============
const int NUM_PAGES = 4;
int currentPage = 0;

// Tilt states
enum TiltState { LEVEL, TILTED_BACK, TILTED_FORWARD };
TiltState tiltState = LEVEL;
unsigned long lastGestureTime = 0;
const unsigned long GESTURE_COOLDOWN = 600;  // ms between gestures

// Thresholds (adjusted for X axis tilt - screen facing up/down)
const float TILT_THRESHOLD = 0.5;    // G force to register tilt
const float LEVEL_THRESHOLD = 0.25;  // G force to consider "returned to level"

// Smoothing
float smoothAccelX = 0;
const float SMOOTH_FACTOR = 0.3;

// Sleep mode
bool displayAsleep = false;
unsigned long lastMovementTime = 0;
const unsigned long SLEEP_TIMEOUT = 5 * 60 * 1000;  // 5 minutes in ms
const float MOVEMENT_THRESHOLD = 0.1;  // G change to detect movement
float prevAccelX = 0, prevAccelY = 0, prevAccelZ = 0;

// Forward declarations
void drawCurrentPage();
void checkSleepWake();

// ============== IMU Functions ==============
void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(QMI8658_ADDR, 1);
  return Wire.read();
}

void initIMU() {
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(10);

  uint8_t id = readReg(0x00);
  Serial.printf("QMI8658 ID: 0x%02X\n", id);

  writeReg(0x02, 0x40);  // CTRL1: auto-increment
  writeReg(0x03, 0x13);  // CTRL2: accel 4G, 1000Hz
  writeReg(0x04, 0x33);  // CTRL3: gyro config
  writeReg(0x08, 0x03);  // CTRL7: enable accel + gyro

  delay(50);
  Serial.println("IMU initialized");
}

void readAccel() {
  Wire.beginTransmission(QMI8658_ADDR);
  Wire.write(0x35);  // AX_L
  Wire.endTransmission(false);
  Wire.requestFrom(QMI8658_ADDR, 6);

  int16_t ax = Wire.read() | (Wire.read() << 8);
  int16_t ay = Wire.read() | (Wire.read() << 8);
  int16_t az = Wire.read() | (Wire.read() << 8);

  accelX = ax * ACCEL_SCALE;
  accelY = ay * ACCEL_SCALE;
  accelZ = az * ACCEL_SCALE;

  // Smooth the X value for gesture detection
  smoothAccelX = (SMOOTH_FACTOR * accelX) + ((1.0 - SMOOTH_FACTOR) * smoothAccelX);
}

// ============== Gesture Detection ==============
void checkGesture() {
  unsigned long now = millis();
  if (now - lastGestureTime < GESTURE_COOLDOWN) return;

  // In landscape mode, X axis = tilt top of screen back/forward
  // Positive X = screen tilted back (facing ceiling)
  // Negative X = screen tilted forward (facing floor)

  switch (tiltState) {
    case LEVEL:
      if (smoothAccelX > TILT_THRESHOLD) {
        tiltState = TILTED_BACK;
        Serial.println("Tilted BACK");
      } else if (smoothAccelX < -TILT_THRESHOLD) {
        tiltState = TILTED_FORWARD;
        Serial.println("Tilted FORWARD");
      }
      break;

    case TILTED_BACK:
      // Returned to level after tilting back = NEXT page
      if (abs(smoothAccelX) < LEVEL_THRESHOLD) {
        currentPage = (currentPage + 1) % NUM_PAGES;
        lastGestureTime = now;
        tiltState = LEVEL;
        Serial.printf("NEXT -> Page %d\n", currentPage + 1);
        drawCurrentPage();
      }
      break;

    case TILTED_FORWARD:
      // Returned to level after tilting forward = PREVIOUS page
      if (abs(smoothAccelX) < LEVEL_THRESHOLD) {
        currentPage = (currentPage - 1 + NUM_PAGES) % NUM_PAGES;
        lastGestureTime = now;
        tiltState = LEVEL;
        Serial.printf("PREV -> Page %d\n", currentPage + 1);
        drawCurrentPage();
      }
      break;
  }
}

// ============== Sleep/Wake Detection ==============
void checkSleepWake() {
  // Calculate movement (change in acceleration)
  float deltaX = abs(accelX - prevAccelX);
  float deltaY = abs(accelY - prevAccelY);
  float deltaZ = abs(accelZ - prevAccelZ);
  float totalMovement = deltaX + deltaY + deltaZ;

  // Store current values for next comparison
  prevAccelX = accelX;
  prevAccelY = accelY;
  prevAccelZ = accelZ;

  unsigned long now = millis();

  if (totalMovement > MOVEMENT_THRESHOLD) {
    // Movement detected
    lastMovementTime = now;

    if (displayAsleep) {
      // Wake up!
      displayAsleep = false;
      lcd.setBrightness(200);
      drawCurrentPage();
      Serial.println("Display woke up!");
    }
  } else {
    // No significant movement
    if (!displayAsleep && (now - lastMovementTime > SLEEP_TIMEOUT)) {
      // Go to sleep
      displayAsleep = true;
      lcd.setBrightness(0);
      Serial.println("Display going to sleep...");
    }
  }
}

// ============== Page Drawing Functions ==============
void drawHeader(const char* title) {
  lcd.fillScreen(TFT_BLACK);

  // Header bar
  lcd.fillRect(0, 0, SCREEN_WIDTH, 28, 0x2104);  // Dark gray
  lcd.setFont(&fonts::DejaVu18);
  lcd.setTextColor(TFT_WHITE, 0x2104);
  lcd.drawString(title, 10, 4);

  // Page indicator
  lcd.setFont(&fonts::DejaVu12);
  lcd.setTextColor(TFT_CYAN, 0x2104);
  char pageStr[10];
  sprintf(pageStr, "%d / %d", currentPage + 1, NUM_PAGES);
  lcd.drawString(pageStr, 260, 7);
}

void drawPage0_Fonts() {
  drawHeader("Fonts");

  int y = 40;
  int col1 = 10, col2 = 170;

  lcd.setFont(&fonts::DejaVu12);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("DejaVu 12", col1, y);

  lcd.setFont(&fonts::FreeSans12pt7b);
  lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  lcd.drawString("FreeSans 12", col2, y);
  y += 28;

  lcd.setFont(&fonts::DejaVu18);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("DejaVu 18", col1, y);

  lcd.setFont(&fonts::FreeSerif12pt7b);
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.drawString("FreeSerif 12", col2, y);
  y += 32;

  lcd.setFont(&fonts::DejaVu24);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("DejaVu 24", col1, y);

  lcd.setFont(&fonts::FreeMono12pt7b);
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.drawString("Mono 12", col2, y);

  // Footer
  lcd.setFont(&fonts::DejaVu12);
  lcd.setTextColor(0x6B6D, TFT_BLACK);
  lcd.drawString("Tilt back + return = Next    Tilt fwd + return = Prev", 20, 150);
}

void drawPage1_Product() {
  drawHeader("Product Info");

  int y = 45;

  lcd.setFont(&fonts::DejaVu24);
  lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  lcd.drawString("ESP32-S3-LCD-1.47B", 10, y);
  y += 35;

  lcd.setFont(&fonts::DejaVu12);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("Waveshare Electronics", 10, y);
  y += 30;

  // Two columns of specs
  int col1 = 10, col2 = 170;

  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.drawString("Display:", col1, y);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("172x320 IPS", col1 + 65, y);

  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.drawString("Flash:", col2, y);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("16 MB", col2 + 50, y);
  y += 22;

  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.drawString("Driver:", col1, y);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("ST7789", col1 + 65, y);

  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.drawString("PSRAM:", col2, y);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("8 MB", col2 + 50, y);
  y += 22;

  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.drawString("IMU:", col1, y);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("QMI8658", col1 + 65, y);

  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.drawString("CPU:", col2, y);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("240 MHz", col2 + 50, y);
}

void drawPage2_System() {
  drawHeader("System Info");

  int y = 45;
  char buf[40];

  lcd.setFont(&fonts::DejaVu12);

  // Memory section
  lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  lcd.drawString("Memory", 10, y);
  lcd.drawFastHLine(10, y + 16, 80, TFT_GREEN);
  y += 25;

  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  sprintf(buf, "Free Heap:  %d KB", ESP.getFreeHeap() / 1024);
  lcd.drawString(buf, 10, y);
  y += 18;

  sprintf(buf, "Free PSRAM: %d KB", ESP.getFreePsram() / 1024);
  lcd.drawString(buf, 10, y);
  y += 18;

  sprintf(buf, "Flash Size: %d MB", ESP.getFlashChipSize() / 1024 / 1024);
  lcd.drawString(buf, 10, y);

  // CPU section (right column)
  y = 45;
  lcd.setTextColor(TFT_CYAN, TFT_BLACK);
  lcd.drawString("Processor", 180, y);
  lcd.drawFastHLine(180, y + 16, 100, TFT_CYAN);
  y += 25;

  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  sprintf(buf, "Model: %s", ESP.getChipModel());
  lcd.drawString(buf, 180, y);
  y += 18;

  sprintf(buf, "Freq:  %d MHz", ESP.getCpuFreqMHz());
  lcd.drawString(buf, 180, y);
  y += 18;

  lcd.drawString("Cores: 2 (Xtensa LX7)", 180, y);
}

void drawPage3_IMU() {
  // Use sprite for flicker-free updates
  sprite.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  sprite.fillSprite(TFT_BLACK);

  // Header
  sprite.fillRect(0, 0, SCREEN_WIDTH, 28, 0x2104);
  sprite.setFont(&fonts::DejaVu18);
  sprite.setTextColor(TFT_WHITE, 0x2104);
  sprite.drawString("IMU Live", 10, 4);

  sprite.setFont(&fonts::DejaVu12);
  sprite.setTextColor(TFT_CYAN, 0x2104);
  char pageStr[10];
  sprintf(pageStr, "%d / %d", currentPage + 1, NUM_PAGES);
  sprite.drawString(pageStr, 260, 7);

  // Accelerometer data
  int y = 45;
  char buf[32];

  sprite.setFont(&fonts::DejaVu18);

  // X axis (used for tilt gestures)
  sprite.setTextColor(TFT_RED, TFT_BLACK);
  sprintf(buf, "X: %+.2f G", accelX);
  sprite.drawString(buf, 10, y);
  // Bar
  sprite.fillRect(150, y + 2, 160, 18, 0x2104);
  sprite.fillRect(150 + 80, y + 4, 2, 14, TFT_WHITE);  // Center line
  int barX = (int)(accelX * 70);
  if (barX > 0) sprite.fillRect(150 + 80, y + 4, min(barX, 78), 14, TFT_RED);
  else sprite.fillRect(150 + 80 + max(barX, -78), y + 4, -max(barX, -78), 14, TFT_RED);
  y += 30;

  // Y axis
  sprite.setTextColor(TFT_GREEN, TFT_BLACK);
  sprintf(buf, "Y: %+.2f G", accelY);
  sprite.drawString(buf, 10, y);
  sprite.fillRect(150, y + 2, 160, 18, 0x2104);
  sprite.fillRect(150 + 80, y + 4, 2, 14, TFT_WHITE);
  int barY = (int)(accelY * 70);
  if (barY > 0) sprite.fillRect(150 + 80, y + 4, min(barY, 78), 14, TFT_GREEN);
  else sprite.fillRect(150 + 80 + max(barY, -78), y + 4, -max(barY, -78), 14, TFT_GREEN);
  y += 30;

  // Z axis
  sprite.setTextColor(TFT_BLUE, TFT_BLACK);
  sprintf(buf, "Z: %+.2f G", accelZ);
  sprite.drawString(buf, 10, y);
  sprite.fillRect(150, y + 2, 160, 18, 0x2104);
  sprite.fillRect(150 + 80, y + 4, 2, 14, TFT_WHITE);
  int barZ = (int)(accelZ * 70);
  if (barZ > 0) sprite.fillRect(150 + 80, y + 4, min(barZ, 78), 14, TFT_BLUE);
  else sprite.fillRect(150 + 80 + max(barZ, -78), y + 4, -max(barZ, -78), 14, TFT_BLUE);
  y += 35;

  // Tilt state indicator
  sprite.setFont(&fonts::DejaVu12);
  sprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  sprite.drawString("Tilt State:", 10, y);

  switch (tiltState) {
    case LEVEL:
      sprite.setTextColor(TFT_WHITE, TFT_BLACK);
      sprite.drawString("LEVEL - Ready", 100, y);
      break;
    case TILTED_BACK:
      sprite.setTextColor(TFT_ORANGE, TFT_BLACK);
      sprite.drawString("TILTED BACK - Return to trigger NEXT", 100, y);
      break;
    case TILTED_FORWARD:
      sprite.setTextColor(TFT_ORANGE, TFT_BLACK);
      sprite.drawString("TILTED FWD - Return to trigger PREV", 100, y);
      break;
  }

  sprite.pushSprite(0, 0);
  sprite.deleteSprite();
}

void drawCurrentPage() {
  switch (currentPage) {
    case 0: drawPage0_Fonts(); break;
    case 1: drawPage1_Product(); break;
    case 2: drawPage2_System(); break;
    case 3: drawPage3_IMU(); break;
  }
}

// ============== Main ==============
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting tilt navigation demo (landscape)...");

  lcd.init();
  lcd.setRotation(1);  // Landscape mode
  lcd.setBrightness(200);

  initIMU();

  // Initialize smoothed value and movement tracking
  readAccel();
  smoothAccelX = accelX;
  prevAccelX = accelX;
  prevAccelY = accelY;
  prevAccelZ = accelZ;
  lastMovementTime = millis();

  drawCurrentPage();
  Serial.println("Ready! Tilt back + return = Next, Tilt forward + return = Prev");
  Serial.println("Display will sleep after 5 minutes of no movement.");
}

void loop() {
  readAccel();
  checkSleepWake();

  // Only process gestures and update display when awake
  if (!displayAsleep) {
    checkGesture();

    // Update IMU page continuously
    if (currentPage == 3) {
      drawPage3_IMU();
    }
  }

  delay(30);
}
