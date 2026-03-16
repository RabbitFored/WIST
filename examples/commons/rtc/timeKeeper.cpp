#include <Arduino.h>
#include <Wire.h>

#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10
#define RTC_ADDR 0x51
#define TCA9554_ADDR 0x20 

// --- BCD TRANSLATORS ---
byte decToBcd(byte val) { return ( (val/10*16) + (val%10) ); }
byte bcdToDec(byte val) { return ( (val/16*10) + (val%16) ); }

void wakeUpHardware() {
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); Wire.write(0x00); 
    Wire.endTransmission();
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); Wire.write(0x00); 
    Wire.endTransmission();
    delay(50);
}

void setRTC_Time(byte second, byte minute, byte hour, byte day, byte month, byte year) {
    Wire.beginTransmission(RTC_ADDR);
    Wire.write(0x04); // Start at Seconds
    Wire.write(decToBcd(second)); // Writing here automatically clears the OS bit!
    Wire.write(decToBcd(minute));
    Wire.write(decToBcd(hour));
    Wire.write(decToBcd(day));
    Wire.write(decToBcd(0)); // Weekday (skipping for this test)
    Wire.write(decToBcd(month));
    Wire.write(decToBcd(year));
    Wire.endTransmission();
}

// --- THE SMART AUTO-SYNC FUNCTION ---
void checkAndSyncRTC() {
    // 1. Read the Seconds register to check the OS (Oscillator Stop) bit
    Wire.beginTransmission(RTC_ADDR);
    Wire.write(0x04); 
    Wire.endTransmission();
    Wire.requestFrom(RTC_ADDR, 1);
    byte seconds_reg = Wire.read();

    // 2. The 8th bit (0x80) is the hardware flag. 
    // If it's 1, the chip lost power and needs the time.
    if (seconds_reg & 0x80) {
        Serial.println("STATUS: RTC Power Lost! Initializing First-Boot Sync...");
        
        // 3. Extract the exact time your laptop compiled the code!
        char s_month[5];
        int year, day, hour, minute, second;
        static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

        sscanf(__DATE__, "%s %d %d", s_month, &day, &year);
        sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);
        int month = (strstr(month_names, s_month) - month_names) / 3 + 1;

        // 4. Inject it into the RTC (Year - 2000 because RTC only wants '26')
        setRTC_Time(second, minute, hour, day, month, year - 2000);
        
        Serial.println("STATUS: >>> RTC SYNCED TO PC CLOCK <<<");
    } else {
        Serial.println("STATUS: RTC Battery Good. Hardware clock is already running.");
    }
}

void setup() {
    Serial.begin(115200);
    while(!Serial);
    
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    wakeUpHardware();
    
    Serial.println("\n--- W.I.S.T. SMART RTC INITIALIZATION ---");

    Wire.beginTransmission(RTC_ADDR);
    Wire.write(0x00); 
    Wire.write(0x00); 
    Wire.endTransmission();

    // The watch now decides for itself if it needs to update the time!
    checkAndSyncRTC(); 
}

void loop() {
    Wire.beginTransmission(RTC_ADDR);
    Wire.write(0x04); 
    Wire.endTransmission();
    Wire.requestFrom(RTC_ADDR, 7);

    if (Wire.available() == 7) {
        byte second  = bcdToDec(Wire.read() & 0x7F);
        byte minute  = bcdToDec(Wire.read() & 0x7F);
        byte hour    = bcdToDec(Wire.read() & 0x3F); 
        byte day     = bcdToDec(Wire.read() & 0x3F);
        Wire.read(); // Skip weekday
        byte month   = bcdToDec(Wire.read() & 0x1F);
        byte year    = bcdToDec(Wire.read());

        char timeStr[64];
        sprintf(timeStr, "20%02d-%02d-%02d | %02d:%02d:%02d", year, month, day, hour, minute, second);
        Serial.println(timeStr);
    } 
    delay(1000); 
}