#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>

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
#define CYAN    0x07FF
#define GREEN   0x07E0
#define RED     0xF800
#define WHITE   0xFFFF
#define BLACK   0x0000
#define YELLOW  0xFFE0

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
    gfx->displayOn();
    gfx->invertDisplay(false); 
    
    // 3. Init Wi-Fi Mode
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    gfx->fillScreen(BLACK);
    gfx->setCursor(110, 200);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(2);
    gfx->println("INITIALIZING\n   SCANNER...");
}

void loop() {
    // 1. Start Scan
    // This blocks the CPU (freezes screen) while scanning
    int n = WiFi.scanNetworks();
    
    // 2. Clear Screen
    gfx->fillScreen(BLACK);
    
    // 3. Header
    gfx->setCursor(120, 40); // Top Center
    gfx->setTextColor(YELLOW);
    gfx->setTextSize(2);
    gfx->println("W.I.S.T. RADAR");
    
    gfx->drawLine(50, 65, 360, 65, RED); // Cool underline

    if (n == 0) {
        gfx->setCursor(100, 200);
        gfx->setTextColor(RED);
        gfx->println("NO SIGNAL FOUND");
    } else {
        // 4. List Networks
        // We only show top 7 to fit the screen
        int count = (n > 7) ? 7 : n;
        
        int yPos = 90; // Start Y position
        
        for (int i = 0; i < count; ++i) {
            // Determine Color based on Signal Strength (RSSI)
            int rssi = WiFi.RSSI(i);
            uint16_t color = GREEN;
            if (rssi < -70) color = YELLOW;
            if (rssi < -85) color = RED;

            // Format: SSID (RSSI)
            // e.g., "HomeWiFi (-60)"
            String ssid = WiFi.SSID(i);
            // Truncate long names
            if(ssid.length() > 14) ssid = ssid.substring(0, 14) + "..";
            
            // Center the text roughly
            gfx->setCursor(60, yPos);
            gfx->setTextColor(color);
            gfx->setTextSize(2);
            gfx->print(ssid);
            
            // Print Signal strength on the right
            gfx->setCursor(300, yPos);
            gfx->setTextColor(WHITE);
            gfx->setTextSize(1);
            gfx->print(rssi);
            gfx->print("dB");

            yPos += 40; // Move down for next line
            delay(10);  // Visual effect
        }
    }
    
    // 5. Wait before next scan
    // Draw a loading bar or just wait
    gfx->setCursor(130, 360);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(1);
    gfx->println("RESCANNING IN 5s...");
    
    delay(5000);
}