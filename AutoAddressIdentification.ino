#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;
byte mpuAddress = 0;

// WHO_AM_I register returns 0x68 on genuine MPU6050
// Some clones return different values
bool isMPU6050(byte address) {
  Wire.beginTransmission(address);
  Wire.write(0x75);  // WHO_AM_I register
  Wire.endTransmission(false);
  Wire.requestFrom(address, (byte)1);
  
  if (Wire.available()) {
    byte whoAmI = Wire.read();
    Serial.printf("  Address 0x%02X -> WHO_AM_I = 0x%02X", address, whoAmI);

    // Accept known WHO_AM_I values
    if (whoAmI == 0x68) { Serial.println(" (Genuine MPU6050)");    return true; }
    if (whoAmI == 0x69) { Serial.println(" (Clone variant)");       return true; }
    if (whoAmI == 0x70) { Serial.println(" (MPU6050 clone)");       return true; }
    if (whoAmI == 0x71) { Serial.println(" (MPU6052C clone)");      return true; }
    if (whoAmI == 0x19) { Serial.println(" (MPU6886 / M5Stack)");   return true; }
    if (whoAmI == 0x38) { Serial.println(" (ICM-20600 clone)");     return true; }

    Serial.println(" (Unknown - skipping)");
  }
  return false;
}

bool scanAndConnect() {
  Serial.println("\nScanning all I2C addresses for MPU6050...\n");

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("Device found at 0x%02X, checking WHO_AM_I...\n", address);
      if (isMPU6050(address)) {
        mpuAddress = address;
        Serial.printf("\nUsing MPU6050 at 0x%02X\n", mpuAddress);
        return true;
      }
    }
  }

  Serial.println("\nNo MPU6050 found! Check wiring.");
  return false;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  delay(1000);

  while (!scanAndConnect()) {
    Serial.println("Retrying in 3 seconds...");
    delay(3000);
  }

  mpu = MPU6050(mpuAddress);
  mpu.initialize();

  Serial.print("Connection test: ");
  Serial.println(mpu.testConnection() ? "PASSED" : "FAILED");
}

void loop() {
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  Serial.printf("=== MPU6050 @ 0x%02X ===\n", mpuAddress);
  Serial.printf("  Accel: X=%6d  Y=%6d  Z=%6d\n", ax, ay, az);
  Serial.printf("  Gyro:  X=%6d  Y=%6d  Z=%6d\n", gx, gy, gz);
  Serial.println();

  delay(500);
}