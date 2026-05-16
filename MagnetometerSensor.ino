// ============================================================
//  XIAO ESP32-S3  —  QMC5883P Compass with Calibration
//  Chip: HP58835712 on GY-271 board  |  I2C addr: 0x2C
//  SDA → D4 (GPIO5)   SCL → D5 (GPIO6)
// ============================================================
//
//  HOW TO CALIBRATE:
//  1. Upload this sketch
//  2. Open Serial Monitor at 115200
//  3. On boot it asks: "Press C to calibrate, any key to skip"
//  4. Press C — then slowly rotate the sensor in ALL directions
//     (tilt forward/back, left/right, spin flat) for ~30 seconds
//  5. Press ENTER when done — offsets are saved to flash (Preferences)
//  6. The saved calibration persists across power cycles
//
// ============================================================

#include <Wire.h>
#include <math.h>
#include <Preferences.h>   // ESP32 NVS flash storage

// ── Pins ─────────────────────────────────────────────────────
#define SDA_PIN  5
#define SCL_PIN  6

// ── QMC5883P ─────────────────────────────────────────────────
#define QMC_ADDR    0x2C
#define REG_XOUT_L  0x01
#define REG_STATUS  0x09
#define REG_MODE    0x0A
#define REG_CONFIG  0x0B

// ── Calibration storage ───────────────────────────────────────
Preferences prefs;

float offX = 0, offY = 0, offZ = 0;
float sclX = 1, sclY = 1, sclZ = 1;
bool  calibrated = false;

// ─────────────────────────────────────────────────────────────
// Register helpers
// ─────────────────────────────────────────────────────────────
void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMC_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(QMC_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(QMC_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

// ─────────────────────────────────────────────────────────────
// Init sensor
// ─────────────────────────────────────────────────────────────
void initSensor() {
  Wire.beginTransmission(QMC_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("  ERROR: QMC5883P not found at 0x2C! Check wiring.");
    while (true) delay(500);
  }
  writeReg(REG_CONFIG, 0x08);
  delay(5);
  writeReg(REG_MODE, 0xCF);  // OSR=512, ±8G, 200Hz, continuous
  delay(10);
  Serial.println("  QMC5883P ✔  (0x2C, 200Hz, ±8G, OSR=512)");
}

// ─────────────────────────────────────────────────────────────
// Read raw XYZ (µT), returns false if not ready
// ─────────────────────────────────────────────────────────────
bool readRaw(float &x, float &y, float &z) {
  if (!(readReg(REG_STATUS) & 0x01)) return false;

  Wire.beginTransmission(QMC_ADDR);
  Wire.write(REG_XOUT_L);
  Wire.endTransmission(false);
  Wire.requestFrom(QMC_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;

  int16_t rx = (int16_t)(Wire.read() | Wire.read() << 8);
  int16_t ry = (int16_t)(Wire.read() | Wire.read() << 8);
  int16_t rz = (int16_t)(Wire.read() | Wire.read() << 8);

  const float scale = 100.0f / 3000.0f;
  x = rx * scale;
  y = ry * scale;
  z = rz * scale;
  return true;
}

// ─────────────────────────────────────────────────────────────
// Load calibration from flash
// ─────────────────────────────────────────────────────────────
void loadCalibration() {
  prefs.begin("compass", true);
  calibrated = prefs.getBool("done", false);
  if (calibrated) {
    offX = prefs.getFloat("offX", 0);
    offY = prefs.getFloat("offY", 0);
    offZ = prefs.getFloat("offZ", 0);
    sclX = prefs.getFloat("sclX", 1);
    sclY = prefs.getFloat("sclY", 1);
    sclZ = prefs.getFloat("sclZ", 1);
  }
  prefs.end();
}

// ─────────────────────────────────────────────────────────────
// Save calibration to flash
// ─────────────────────────────────────────────────────────────
void saveCalibration() {
  prefs.begin("compass", false);
  prefs.putBool ("done", true);
  prefs.putFloat("offX", offX);
  prefs.putFloat("offY", offY);
  prefs.putFloat("offZ", offZ);
  prefs.putFloat("sclX", sclX);
  prefs.putFloat("sclY", sclY);
  prefs.putFloat("sclZ", sclZ);
  prefs.end();
}

// ─────────────────────────────────────────────────────────────
// Interactive calibration routine
// ─────────────────────────────────────────────────────────────
void runCalibration() {
  Serial.println();
  Serial.println("╔══════════════════════════════════════════════════╗");
  Serial.println("║             COMPASS CALIBRATION                  ║");
  Serial.println("╠══════════════════════════════════════════════════╣");
  Serial.println("║  Slowly rotate the sensor in ALL directions:     ║");
  Serial.println("║  • Spin it flat (like a compass on a table)      ║");
  Serial.println("║  • Tilt it nose-up, nose-down                    ║");
  Serial.println("║  • Roll it left side up, right side up           ║");
  Serial.println("║  Keep going for 30 seconds for best results.     ║");
  Serial.println("║  Press ENTER when done.                          ║");
  Serial.println("╚══════════════════════════════════════════════════╝");
  Serial.println();

  while (Serial.available()) Serial.read();

  float mnX =  1e9, mxX = -1e9;
  float mnY =  1e9, mxY = -1e9;
  float mnZ =  1e9, mxZ = -1e9;
  unsigned long sampleCount = 0;
  unsigned long lastStatus  = 0;

  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') break;
    }

    float x, y, z;
    if (readRaw(x, y, z)) {
      mnX = min(mnX, x);  mxX = max(mxX, x);
      mnY = min(mnY, y);  mxY = max(mxY, y);
      mnZ = min(mnZ, z);  mxZ = max(mxZ, z);
      sampleCount++;
    }

    if (millis() - lastStatus > 2000) {
      lastStatus = millis();
      Serial.printf("  Samples: %lu  |  X[%6.1f…%6.1f]  Y[%6.1f…%6.1f]  Z[%6.1f…%6.1f]\n",
        sampleCount, mnX, mxX, mnY, mxY, mnZ, mxZ);
    }
    delay(5);
  }

  if (sampleCount < 50) {
    Serial.println("  Too few samples — calibration cancelled. Try again.");
    return;
  }

  offX = (mxX + mnX) / 2.0f;
  offY = (mxY + mnY) / 2.0f;
  offZ = (mxZ + mnZ) / 2.0f;

  float rngX = (mxX - mnX) / 2.0f;
  float rngY = (mxY - mnY) / 2.0f;
  float rngZ = (mxZ - mnZ) / 2.0f;
  float avgRng = (rngX + rngY + rngZ) / 3.0f;

  sclX = (rngX > 0) ? avgRng / rngX : 1.0f;
  sclY = (rngY > 0) ? avgRng / rngY : 1.0f;
  sclZ = (rngZ > 0) ? avgRng / rngZ : 1.0f;

  saveCalibration();
  calibrated = true;

  Serial.println();
  Serial.println("  ── Calibration result ─────────────────────────");
  Serial.printf("     Hard-iron offset X: %7.3f µT\n", offX);
  Serial.printf("     Hard-iron offset Y: %7.3f µT\n", offY);
  Serial.printf("     Hard-iron offset Z: %7.3f µT\n", offZ);
  Serial.printf("     Soft-iron scale  X: %7.4f\n",    sclX);
  Serial.printf("     Soft-iron scale  Y: %7.4f\n",    sclY);
  Serial.printf("     Soft-iron scale  Z: %7.4f\n",    sclZ);
  Serial.println("  ✔  Saved to flash — persists after power-off.");
  Serial.println();
}

// ─────────────────────────────────────────────────────────────
// Compass direction (16-point)
// ─────────────────────────────────────────────────────────────
const char* compassDir(float deg) {
  const char* d[] = {
    "N ","NE","E ","SE","S ","SW","W ","NW"
  };
  int idx = (int)((deg + 22.5f) / 45.0f) % 8;
  return d[idx];
}

// ─────────────────────────────────────────────────────────────
// ASCII compass rose
// ─────────────────────────────────────────────────────────────
void printCompassRose(float heading) {
  int pos = (int)((heading + 22.5f) / 45.0f) % 8;
  const char* points[8] = {
    "N","NE","E","SE","S","SW","W","NW"
  };
  Serial.print("  [");
  for (int i = 0; i < 8; i++) Serial.print(i == pos ? "█" : "─");
  Serial.printf("]  %.1f°  %s\n", heading, points[pos]);
}

// ─────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("  ╔══════════════════════════════════════════╗");
  Serial.println("  ║   XIAO ESP32-S3  ×  QMC5883P Compass    ║");
  Serial.println("  ║   HP58835712 IC  |  addr 0x2C           ║");
  Serial.println("  ╚══════════════════════════════════════════╝");
  Serial.println();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  initSensor();

  loadCalibration();
  if (calibrated) {
    Serial.println("  ✔  Calibration loaded from flash.");
    Serial.printf("     offX=%.3f  offY=%.3f  offZ=%.3f\n", offX, offY, offZ);
    Serial.printf("     sclX=%.4f sclY=%.4f sclZ=%.4f\n",   sclX, sclY, sclZ);
  } else {
    Serial.println("  ⚠  No calibration found — heading may be inaccurate.");
  }

  Serial.println();
  Serial.println("  Press C + ENTER to (re)calibrate.");
  Serial.println("  Press any other key + ENTER to skip.");
  Serial.println();

  unsigned long waitStart = millis();
  while (millis() - waitStart < 5000) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'C' || c == 'c') runCalibration();
      break;
    }
    if ((millis() - waitStart) % 1000 < 20) Serial.print(".");
  }
  Serial.println();

  Serial.println("── Live Readings ──────────────────────────────────────────");
  Serial.println("   X (µT)     Y (µT)     Z (µT)    Heading   Direction");
  Serial.println("──────────────────────────────────────────────────────────");
}

// ─────────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────────
unsigned long lastPrint = 0;

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'C' || c == 'c') {
      runCalibration();
      Serial.println("── Live Readings ──────────────────────────────────────────");
      Serial.println("   X (µT)     Y (µT)     Z (µT)    Heading   Direction");
      Serial.println("──────────────────────────────────────────────────────────");
    }
  }

  float rx, ry, rz;
  if (!readRaw(rx, ry, rz)) { delay(5); return; }

  float x = (rx - offX) * sclX;
  float y = (ry - offY) * sclY;
  float z = (rz - offZ) * sclZ;

  float heading = atan2f(y, x) * 180.0f / (float)M_PI;
  if (heading < 0) heading += 360.0f;

  if (millis() - lastPrint >= 250) {
    lastPrint = millis();
    Serial.printf("   %9.3f  %9.3f  %9.3f   %6.1f°   %s\n",
      x, y, z, heading, compassDir(heading));
    printCompassRose(heading);
    Serial.println("──────────────────────────────────────────────────────────");
  }
}
