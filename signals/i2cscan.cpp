#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

extern "C" {
    #include <lvgl.h>
}

// --- W.I.S.T. HARDWARE MAP (1.46B) ---
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

// --- OBJECTS ---
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);
Arduino_GFX *gfx = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);

// --- LVGL CONSTANTS ---
static const uint32_t screenWidth  = 412;
static const uint32_t screenHeight = 412;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;

// --- DISPLAY BRIDGE ---
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(disp_drv);
}

// --- HARDWARE WAKE & SCANNER ---
void wakeUpHardware() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000); 
    
    // Config Output
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); Wire.write(0x00); 
    Wire.endTransmission();
    
    // Reset Sequence
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); Wire.write(0x00); 
    Wire.endTransmission();
    delay(50);
    
    // Wake up LCD and Touch IC
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); 
    Wire.write(EXIO_LCD_RST | EXIO_TP_RST); 
    Wire.endTransmission();
    delay(150); // Extra delay to ensure Touch IC boots up completely
}

void scanI2CBus() {
    Serial.println("\n--- STARTING I2C HARDWARE SCAN ---");
    byte error, address;
    int nDevices = 0;

    for(address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            Serial.printf("--> DEVICE FOUND AT ADDRESS: 0x%02X\n", address);
            nDevices++;
        } else if (error == 4) {
            Serial.printf("--> UNKNOWN ERROR AT ADDRESS: 0x%02X\n", address);
        }
    }
    
    if (nDevices == 0) {
        Serial.println("--> NO I2C DEVICES FOUND!");
    } else {
        Serial.println("--- SCAN COMPLETE ---\n");
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Wait for Serial Monitor to connect

    // 1. Wake Hardware & Run Diagnostics
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    
    wakeUpHardware();
    scanI2CBus(); // RUN THE SCANNER!

    // 2. Start Display
    if (!gfx->begin()) {
        Serial.println("GFX Init Failed!");
        while(1);
    }
    gfx->displayOn();
    gfx->invertDisplay(false);

    // 3. Initialize LVGL
    lv_init();

    // 4. Setup Display Buffer
    buf1 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 10 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, screenWidth * screenHeight / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 5. Diagnostic UI
    lv_obj_t * screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0); // Black background
    
    lv_obj_t * label = lv_label_create(screen);
    lv_label_set_text(label, "DIAGNOSTICS RUNNING\n\nCHECK SERIAL MONITOR\nFOR I2C ADDRESSES");
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0); // Hacker Green
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void loop() {
    lv_timer_handler(); 
    delay(5);
}


/// 
/// --- STARTING I2C HARDWARE SCAN ---
/// --> DEVICE FOUND AT ADDRESS: 0x20 [ TCA9554 IO Expander - Power Switch ]
/// --> DEVICE FOUND AT ADDRESS: 0x51 [ PCF85063 Real-Time Clock (RTC) - Timekeeper ]
/// --> DEVICE FOUND AT ADDRESS: 0x6B [ QMI8658 6-Axis IMU - Motion Sensor ]
/// --> DEVICE FOUND AT ADDRESS: 0x7E
/// --- SCAN COMPLETE ---