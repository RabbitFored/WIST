#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <math.h>

// --- W.I.S.T. HARDWARE MAP (1.46B) ---
#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10

// QSPI Display Pins
#define TFT_CS    21
#define TFT_SCK   40
#define TFT_D0    46 
#define TFT_D1    45
#define TFT_D2    42
#define TFT_D3    41

// Power
#define PIN_LCD_BL   5
#define TCA9554_ADDR 0x20 
#define EXIO_TP_RST  (1 << 1)
#define EXIO_LCD_RST (1 << 2)

// --- COLORS ---
#define RED     0xF800
#define DARKRED 0x7800
#define WHITE   0xFFFF
#define BLACK   0x0000

// --- OBJECTS ---
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);
Arduino_GFX *gfx = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);

void wakeUpHardware() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL); 
    
    // Config Output
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); Wire.write(0x00); 
    Wire.endTransmission();

    // Reset Sequence
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); Wire.write(0x00); // Low
    Wire.endTransmission();
    delay(50);

    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); 
    Wire.write(EXIO_LCD_RST | EXIO_TP_RST); // High
    Wire.endTransmission();
    delay(100);
}

// --- THE HEART FORMULA ---
void drawHeart(int cx, int cy, float scale, uint16_t color) {
    // We draw points along the curve
    for (float t = 0; t < 2 * PI; t += 0.01) {
        float x = 16 * pow(sin(t), 3);
        float y = 13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t);
        
        // Scale and Center
        // Note: Y is inverted on screens, so we subtract y
        int plotX = cx + (x * scale);
        int plotY = cy - (y * scale);
        
        // Draw a small circle at each point to make the line thick
        gfx->fillCircle(plotX, plotY, 2, color);
    }
}

// Function to fill the heart (Draw concentric hearts)
void fillHeart(int cx, int cy, float maxScale, uint16_t color) {
    for (float s = 0.5; s < maxScale; s += 0.5) {
        drawHeart(cx, cy, s, color);
    }
}

void setup() {
    Serial.begin(115200);

    // 1. Wake Hardware
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    wakeUpHardware();

    // 2. Init Display
    if (!gfx->begin()) {
        while(1);
    }
    
    // 3. Set Orientation
    gfx->displayOn();
    gfx->invertDisplay(false); // Validated
    
    // 4. Initial Background
    gfx->fillScreen(BLACK);
    
    // 5. Draw Static Text
    gfx->setCursor(140, 100);
    gfx->setTextColor(WHITE);
    gfx->setTextSize(3);
    gfx->println("");
    
    gfx->setCursor(100, 330); // Bottom
    gfx->println("HAPPY VALENTINE'S");
}

void loop() {
    int cx = 206;
    int cy = 206;
    
    // EXPAND (Systole)
    for(float s = 8.0; s <= 10.0; s += 0.5) {
        fillHeart(cx, cy, s, RED);
        delay(20);
    }
    
    // CONTRACT (Diastole)
    // We erase the edges by drawing black concentric hearts from outside in
    for(float s = 10.0; s >= 8.0; s -= 0.5) {
        drawHeart(cx, cy, s, BLACK); // Erase edge
        delay(20);
    }
    
    // Double Beat Pause
    delay(100);
    
    // Second Beat (Faster)
    for(float s = 8.0; s <= 9.0; s += 0.5) {
        fillHeart(cx, cy, s, RED);
        delay(10);
    }
    for(float s = 9.0; s >= 8.0; s -= 0.5) {
        drawHeart(cx, cy, s, BLACK);
        delay(10);
    }
    
    delay(800); // Rest
}