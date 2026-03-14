#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include "time.h"

// --- CREDENTIALS (REQUIRED) ---
const char* ssid     = "DIRECT-NS-MOX";  
const char* password = "12345678";  
// --- NTP SETTINGS ---
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // IST (UTC +5:30)
const int   daylightOffset_sec = 0;

// --- HARDWARE PINOUT (Waveshare 1.46B) ---
#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10
#define TFT_CS    21
#define TFT_SCK   40
#define TFT_D0    46 
#define TFT_D1    45
#define TFT_D2    42
#define TFT_D3    41
#define PIN_LCD_BL   5
#define TCA9554_ADDR 0x20 
#define EXIO_TP_RST  (1 << 1)
#define EXIO_LCD_RST (1 << 2)

// --- COLORS ---
#define CYAN    0x07FF
#define WHITE   0xFFFF
#define BLACK   0x0000
#define DARKGREY 0x2104 // Subtle background for UI elements

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);
Arduino_GFX *gfx = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);

// Global variable to track time and prevent unnecessary redraws
int last_second = -1;

void wakeUpHardware() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL); 
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); Wire.write(0x00); // Config Output
    Wire.endTransmission();
    
    // Reset Sequence
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); Wire.write(0x00); // Low
    Wire.endTransmission();
    delay(50);
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); Wire.write(EXIO_LCD_RST | EXIO_TP_RST); // High
    Wire.endTransmission();
    delay(100);
}

void setup() {
    Serial.begin(115200);

    // 1. Hardware Init
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    wakeUpHardware();

    if (!gfx->begin()) { while(1); }
    gfx->displayOn();
    gfx->invertDisplay(false); 
    gfx->fillScreen(BLACK);

    // 2. Connecting UI
    gfx->setCursor(110, 180);
    gfx->setTextColor(CYAN);
    gfx->setTextSize(2);
    gfx->println("ESTABLISHING\n   UPLINK...");

    // 3. Wi-Fi Connection
    WiFi.begin(ssid, password);
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        // Simple loading animation
        gfx->setCursor(180 + (dots*10), 240);
        gfx->print(".");
        dots++;
        if(dots > 5) {
             dots = 0;
             gfx->fillRect(180, 240, 60, 20, BLACK); // Clear dots
        }
    }

    // 4. Time Sync
    gfx->fillScreen(BLACK);
    gfx->setCursor(100, 200);
    gfx->println("SYNCING CLOCK...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Wait for time to actually be set
    struct tm timeinfo;
    while(!getLocalTime(&timeinfo)){
        Serial.println("Retrying NTP...");
        delay(500);
    }
    
    // 5. Final Clear before Watch Face
    gfx->fillScreen(BLACK);
    
    // Draw Static Interface Elements (Things that don't change)
    gfx->drawCircle(206, 206, 200, DARKGREY); // Outer Ring
    gfx->drawCircle(206, 206, 195, DARKGREY); // Inner Ring
    
    gfx->setCursor(155, 120);
    gfx->setTextColor(CYAN); // No background needed for static text
    gfx->setTextSize(2);
    gfx->print("W.I.S.T.");
}

void loop() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        
        // ONLY update if the second has changed
        if (timeinfo.tm_sec != last_second) {
            last_second = timeinfo.tm_sec;

            // --- DRAW TIME ---
            // Center roughly for 412 width. 
            // TextSize 7 is BIG. Each char is approx 42px wide.
            // 00:00 is 5 chars. ~210px width.
            
            gfx->setCursor(65, 180);
            gfx->setTextSize(7);
            
            // MAGIC TRICK: WHITE text on BLACK background
            // This overwrites the previous pixels directly.
            gfx->setTextColor(WHITE, BLACK); 
            
            gfx->printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

            // --- DRAW SECONDS (Smaller) ---
            gfx->setCursor(170, 260);
            gfx->setTextSize(3);
            gfx->setTextColor(CYAN, BLACK);
            gfx->printf(":%02d", timeinfo.tm_sec);
            
            // --- DRAW DATE ---
            gfx->setCursor(120, 320);
            gfx->setTextSize(2);
            gfx->setTextColor(DARKGREY, BLACK);
            gfx->printf("%02d / %02d", timeinfo.tm_mday, timeinfo.tm_mon + 1);
        }
    }
    
    // Small delay to prevent CPU hogging, but fast enough to catch the second change
    delay(100); 
}