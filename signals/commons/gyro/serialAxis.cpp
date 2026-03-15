#include <Arduino.h>
#include <Wire.h>

#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10
#define QMI8658_ADDR 0x6B

void setup() {
    Serial.begin(115200);
    while (!Serial);
    
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Serial.println("W.I.S.T. IMU INITIALIZATION...");

    // 1. Verify the Chip ID (WHO_AM_I should return 0x05)
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x00);
    Wire.endTransmission();
    Wire.requestFrom(QMI8658_ADDR, 1);
    if (Wire.available()) {
        byte whoami = Wire.read();
        Serial.print("IMU Identity Code: 0x");
        Serial.println(whoami, HEX); 
    }

    // 2. Enable Auto-Increment (Allows us to read XYZ in one fast burst)
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x02); // CTRL1 Register
    Wire.write(0x60); 
    Wire.endTransmission();

    // 3. Configure Accelerometer (1KHz polling, 8g scale)
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x03); // CTRL2 Register
    Wire.write(0x23); 
    Wire.endTransmission();

    // 4. Turn the Sensors ON
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x08); // CTRL7 Register
    Wire.write(0x03); // Enable Accel & Gyro
    Wire.endTransmission();
    
    Serial.println("SENSORS ACTIVE. Streaming telemetry...");
}

void loop() {
    // Start reading at the Accel X-Axis Low Byte register (0x35)
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x35); 
    Wire.endTransmission();
    
    // Request 6 bytes of data (X-low, X-high, Y-low, Y-high, Z-low, Z-high)
    Wire.requestFrom(QMI8658_ADDR, 6); 

    if (Wire.available() == 6) {
        // Stitch the bytes together into 16-bit integers
        int16_t ax = Wire.read() | (Wire.read() << 8);
        int16_t ay = Wire.read() | (Wire.read() << 8);
        int16_t az = Wire.read() | (Wire.read() << 8);

        Serial.print("X-Axis: "); Serial.print(ax);
        Serial.print("\t| Y-Axis: "); Serial.print(ay);
        Serial.print("\t| Z-Axis: "); Serial.println(az);
    }
    
    delay(100); // Poll 10 times a second
}