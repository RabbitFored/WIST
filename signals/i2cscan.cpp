#include <Arduino.h>
#include <Wire.h>

#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10

void setup() {
    Serial.begin(115200);
    while (!Serial); // Wait for serial monitor to open
    Serial.println("\n--- W.I.S.T. INTERNAL I2C DIAGNOSTIC ---");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
}

void loop() {
    byte error, address;
    int nDevices = 0;

    Serial.println("Scanning internal bus...");

    for(address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("Device found at address 0x");
            if (address < 16) {
                Serial.print("0");
            }
            Serial.println(address, HEX);
            nDevices++;
        }
        else if (error == 4) {
            Serial.print("Unknown error at address 0x");
            if (address < 16) {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }    
    }
    
    if (nDevices == 0) {
        Serial.println("No I2C devices found.\n");
    } else {
        Serial.println("Scan complete.\n");
    }

    delay(5000); // Wait 5 seconds for next scan
}