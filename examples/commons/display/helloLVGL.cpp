#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

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

// --- LVGL DISPLAY BUFFER ---
// We allocate a buffer for 1/10th of the screen (412x412). 
// Doing this in PSRAM prevents crashes.
static const uint32_t screenWidth  = 412;
static const uint32_t screenHeight = 412;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;

// --- THE BRIDGE: LVGL -> GFX ---
// LVGL calls this function when it is ready to push pixels to the screen
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    // Push the calculated pixels via QSPI
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);

    // Tell LVGL we are done
    lv_disp_flush_ready(disp_drv);
}


void wakeUpHardware() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL); 
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); Wire.write(0x00); 
    Wire.endTransmission();
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); Wire.write(0x00); 
    Wire.endTransmission();
    delay(50);
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); 
    Wire.write(EXIO_LCD_RST | EXIO_TP_RST); 
    Wire.endTransmission();
    delay(100);
}

void setup() {
    Serial.begin(115200);

    // 1. Hardware Wake & GFX Init
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    wakeUpHardware();

    if (!gfx->begin()) {
        Serial.println("GFX Init Failed!");
        while(1);
    }
    gfx->displayOn();
    gfx->invertDisplay(false);

    // 2. Initialize LVGL
    lv_init();

    // 3. Allocate PSRAM Buffer
    buf1 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 10 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, screenWidth * screenHeight / 10);

    // 4. Register Display Driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush; // Connect the bridge
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 5. Create "Hello" UI
    // Instead of raw math, we just create a label object!
    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello World!");
    lv_obj_set_style_text_color(label, lv_color_hex(0xA83232), 0); // Cyan
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0); // Perfect center automatically!
}

void loop() {
    // Let LVGL handle all animations, buttons, and updates
    lv_timer_handler(); 
    delay(5);
}