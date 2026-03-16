#include <Arduino.h>
#include <Wire.h>

// --- W.I.S.T. HARDWARE DEFINITIONS ---
// I2C Bus (Updated from Datasheet)
#define I2C_SDA_PIN 11
#define I2C_SCL_PIN 10

// Direct Backlight Pin (Not on Expander for 1.46")
#define PIN_LCD_BL_GPIO 5

// The Ganglion (IO Expander)
#define TCA9554_ADDR 0x20 

// Registers
#define REG_INPUT_PORT  0x00
#define REG_OUTPUT_PORT 0x01
#define REG_CONFIG      0x03

// Pin Mapping on TCA9554 (1.46" Specific)
// Bit 1 = Touch Reset, Bit 2 = LCD Reset
#define EXIO_TP_RST     (1 << 1) // 0x02
#define EXIO_LCD_RST    (1 << 2) // 0x04

// Helper to send commands to the IO Expander
void writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(reg);
    Wire.write(value);
    uint8_t error = Wire.endTransmission();
    if (error) Serial.printf("ERROR: I2C Fail (Code %d)\n", error);
}

void setup() {
    // 1. Initialize Native USB
    Serial.begin(115200);
    delay(3000); 
    Serial.println("\n\n--- W.I.S.T. OS v0.2: PINOUT CORRECTION ---");

    // 2. Setup Direct Backlight (GPIO 5)
    pinMode(PIN_LCD_BL_GPIO, OUTPUT);
    digitalWrite(PIN_LCD_BL_GPIO, LOW); // Start Dark

    // 3. Start I2C (New Pins 11/10)
    Serial.println("SYSTEM: Initializing I2C Bus (SDA:11, SCL:10)...");
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // 4. Ping the Expander
    Wire.beginTransmission(TCA9554_ADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("SUCCESS: IO Expander (0x20) Found!");
    } else {
        Serial.println("CRITICAL: IO Expander STILL MISSING.");
        Serial.println("Check: Is the battery connected? (Some rails need bat)");
        while(1) { 
            // Panic Blink on the raw GPIO just in case
            digitalWrite(PIN_LCD_BL_GPIO, !digitalRead(PIN_LCD_BL_GPIO));
            delay(200); 
        } 
    }

    // 5. Configure Expander as OUTPUT
    writeRegister(REG_CONFIG, 0x00);

    // 6. Release the Reset Hold
    // We must set Bit 1 and Bit 2 HIGH to let the screen/touch boot.
    Serial.println("SYSTEM: Releasing LCD Reset...");
    writeRegister(REG_OUTPUT_PORT, EXIO_TP_RST | EXIO_LCD_RST);
}

void loop() {
    // The Heartbeat (Now toggling GPIO 5)
    Serial.println("STATUS: Lights ON!");
    digitalWrite(PIN_LCD_BL_GPIO, HIGH);
    delay(1000);

    Serial.println("STATUS: Lights OFF!");
    digitalWrite(PIN_LCD_BL_GPIO, LOW);
    delay(1000);
}