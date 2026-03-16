#include <Arduino.h>
#include <Wire.h>

#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10
#define QMI8658_ADDR 0x6B
#define TCA9554_ADDR 0x20 
#define PIN_LCD_BL 5

// --- SLEEP STATE MACHINE VARIABLES ---
bool is_awake = true;
bool was_looking = false;
uint32_t awake_timer = 0;
uint32_t lowered_timer = 0;

void wakeUpHardware() {
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); Wire.write(0x00); 
    Wire.endTransmission();
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); Wire.write(0x00); 
    Wire.endTransmission();
    delay(50);
}

void init_IMU() {
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x02); Wire.write(0x60); 
    Wire.endTransmission();
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x03); Wire.write(0x23); 
    Wire.endTransmission();
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x08); Wire.write(0x03); 
    Wire.endTransmission();
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH); 
    
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    wakeUpHardware();
    init_IMU();
    
    Serial.println("--- W.I.S.T. COMMERCIAL RAISE-TO-WAKE ---");
    awake_timer = millis();
}

void loop() {
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x35); 
    Wire.endTransmission();
    Wire.requestFrom(QMI8658_ADDR, 6); 

    if (Wire.available() == 6) {
        int16_t ax = Wire.read() | (Wire.read() << 8);
        int16_t ay = Wire.read() | (Wire.read() << 8);
        int16_t az = Wire.read() | (Wire.read() << 8); 

        // Is the user currently looking at the watch?
        bool is_looking = (az < -2000); 

        // 1. THE WAKE TRIGGER (Edge Detection)
        if (is_looking && !was_looking) {
            if (!is_awake) {
                Serial.println(">>> RAISE DETECTED: Waking Display <<<");
                digitalWrite(PIN_LCD_BL, HIGH);
                is_awake = true;
            }
            awake_timer = millis(); // Start the strict 5-second countdown
            lowered_timer = 0;      // Reset lowered tracker
        }

        // 2. THE SLEEP TRIGGERS (Only check these if the screen is currently ON)
        if (is_awake) {
            // Trigger A: Absolute 5-Second Timeout (Even if still looking)
            if (millis() - awake_timer > 5000) {
                Serial.println(">>> TIMEOUT: Display Sleeping <<<");
                digitalWrite(PIN_LCD_BL, LOW);
                is_awake = false;
            }
            // Trigger B: Quick Sleep (User lowered wrist before 5 seconds)
            else if (!is_looking) {
                if (lowered_timer == 0) {
                    lowered_timer = millis(); // Start a 1-second debounce clock
                }
                // If wrist has been down for a full second, kill the screen early
                else if (millis() - lowered_timer > 1000) {
                    Serial.println(">>> WRIST LOWERED: Display Sleeping <<<");
                    digitalWrite(PIN_LCD_BL, LOW);
                    is_awake = false;
                }
            } else {
                // If they are still looking, reset the lowered debounce clock
                lowered_timer = 0; 
            }
        }

        was_looking = is_looking; // Save the state for the next loop
    }
    
    // Smooth polling when awake, battery-saving crawl when asleep
    if (is_awake) { delay(50); } 
    else { delay(250); }
}