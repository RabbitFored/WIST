#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

// --- W.I.S.T. HARDWARE MAP (Model 1.46B) ---
//

// 1. THE NERVE (I2C Bus)
// Used for IO Expander and Touch
#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10

// 2. THE VISUAL CORTEX (QSPI Video)
// These are the specific pins for the 1.46B board
#define TFT_CS    21
#define TFT_SCK   40
#define TFT_D0    46 
#define TFT_D1    45
#define TFT_D2    42
#define TFT_D3    41

// 3. THE HEART (Power)
#define PIN_LCD_BL   5
#define TCA9554_ADDR 0x20 
// Expander Bits: Bit 1=Touch_RST, Bit 2=LCD_RST
#define EXIO_TP_RST  (1 << 1)
#define EXIO_LCD_RST (1 << 2)

// --- GRAPHICS OBJECT ---
// 1. Data Bus (QSPI)
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);

// 2. Driver (SPD2010 for 412x412)
Arduino_GFX *gfx = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);

void wakeUpScreen() {
    Serial.println("SYSTEM: Initializing I2C (Pins 11/10)...");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL); 
    
    // 1. Configure Expander (Output Mode)
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); Wire.write(0x00); 
    Wire.endTransmission();

    // 2. Reset Sequence (Low -> High)
    Serial.println("SYSTEM: Resetting Screen...");
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); Wire.write(0x00); // Low
    Wire.endTransmission();
    delay(100);

    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); 
    Wire.write(EXIO_LCD_RST | EXIO_TP_RST); // High
    Wire.endTransmission();
    delay(200);
    
    // Note: We DO NOT need to kill I2C here because the pins are separate!
    // I2C is on 11/10. Video is on 40-46. No conflict!
}

void setup() {
    Serial.begin(115200);
    // delay(2000); 

    Serial.println("--- W.I.S.T. OS v3.0: 1.46B BOOT ---");

    // 1. Backlight
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);

    // 2. Wake Hardware
    wakeUpScreen();

    // 3. Start Graphics
    // 412x412 is a lot of pixels, we run at 80MHz if possible
    if (!gfx->begin()) {
        Serial.println("CRITICAL: GFX Init Failed!");
        while(1);
    }
    
    // 4. Initialization Fixes
    gfx->displayOn();
    gfx->invertDisplay(true); // Usually needed for these IPS rounds
    
    Serial.println("SYSTEM: Video Bus Stable. Drawing...");
    
    // 5. TEST PATTERN
    gfx->fillScreen(0x0000); // Black
    
    // Draw ARC Reactor (Cyan)
    // Center is 206 (412/2)
    gfx->fillCircle(206, 206, 180, 0x07FF); 
    gfx->drawCircle(206, 206, 190, 0xFFFF);
    
    gfx->setCursor(140, 180);
    gfx->setTextColor(0xFFFF); // White
    gfx->setTextSize(4);
    gfx->println("W.I.S.T.");
    
    gfx->setCursor(120, 230);
    gfx->setTextSize(2);
    gfx->println("Hello World!");
}

void loop() {
    // Wave Pulse
    for(int r=180; r<190; r++) {
        gfx->drawCircle(206, 206, r, 0x07FF);
        delay(20);
    }
    for(int r=180; r<190; r++) {
        gfx->drawCircle(206, 206, r, 0x0000);
        delay(20);
    }
}