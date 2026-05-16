#include <Wire.h>
#include <VL53L0X.h>

VL53L0X sensor;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  sensor.setTimeout(500);

  if (!sensor.init()) {
    Serial.println("Failed to detect VL53L0X!");
    while (1) {}
  }

  // ─── PICK ONE MODE ──────────────────────────────────────────

  // 1. DEFAULT — best all-around (no changes needed)

  // 2. LONG RANGE — up to ~2 m, less accurate
  // sensor.setSignalRateLimit(0.1);
  // sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  // sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);

  // 3. HIGH ACCURACY — slower (~5 reads/sec), tightest results
  // sensor.setMeasurementTimingBudget(200000);

  // 4. HIGH SPEED — ~50 reads/sec, noisier
  // sensor.setMeasurementTimingBudget(20000);

  // ─── PICK ONE READ STYLE ────────────────────────────────────

  // A. CONTINUOUS — fastest, best for most projects (recommended)
  sensor.startContinuous();

  // B. SINGLE SHOT — lower power, no startContinuous() needed
  //    (just comment out the line above, nothing else to add here)

  // ────────────────────────────────────────────────────────────
}

void loop() {

  // Use this for CONTINUOUS mode (option A above)
  uint16_t distance = sensor.readRangeContinuousMillimeters();

  // Use this instead for SINGLE SHOT mode (option B above)
  // uint16_t distance = sensor.readRangeSingleMillimeters();

  if (sensor.timeoutOccurred()) {
    Serial.println("Timeout!");
  } else {
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" mm");
  }

  delay(100);   // increase to 500+ if using single-shot to save power
}
