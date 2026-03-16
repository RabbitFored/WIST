#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <driver/rtc_io.h> 

// --- W.I.S.T. HARDWARE MAP (1.46B) ---
#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10
#define PIN_PWR_BTN 6     
#define PIN_BAT_LATCH 7   
#define PIN_BAT_ADC 8     // Extracted from official driver

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
Arduino_DataBus *bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);
Arduino_GFX *gfx = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);

static const uint32_t screenWidth  = 412;
static const uint32_t screenHeight = 412;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;

lv_obj_t * main_label; 
lv_obj_t * bat_label; 

bool ignore_initial_press = true; 

// --- THE BRIDGE: LVGL -> GFX ---
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
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

// --- BATTERY LOGIC ---
void update_battery_status() {
    int mv = analogReadMilliVolts(PIN_BAT_ADC);
    // Applied math from official driver
    float voltage = (mv * 3.0 / 1000.0) / 0.990476; 
    
    int percent = 0;
    if (voltage >= 4.15) percent = 100;
    else if (voltage <= 3.30) percent = 0;
    else percent = (int)(((voltage - 3.30) / (4.15 - 3.30)) * 100.0);
    
    if (percent > 100) percent = 100;
    if (percent < 0) percent = 0;

    char bat_str[32];
    sprintf(bat_str, "BAT: %d%% (%.2fV)", percent, voltage);
    lv_label_set_text(bat_label, bat_str);
}

// --- SHUTDOWN PROTOCOL ---
void execute_shutdown() {
    lv_obj_add_flag(bat_label, LV_OBJ_FLAG_HIDDEN); 
    lv_label_set_text(main_label, "SYSTEM\nOFF");
    lv_obj_set_style_text_color(main_label, lv_color_hex(0xFF0000), 0); 
    lv_refr_now(NULL); 
    delay(800); 

    digitalWrite(PIN_LCD_BL, LOW);
    rtc_gpio_pullup_en((gpio_num_t)PIN_PWR_BTN);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_PWR_BTN);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PWR_BTN, 0);
    digitalWrite(PIN_BAT_LATCH, LOW); 
    esp_deep_sleep_start();           
}

void setup() {
    pinMode(PIN_PWR_BTN, INPUT_PULLUP);
    pinMode(PIN_BAT_LATCH, OUTPUT);
    pinMode(PIN_BAT_ADC, INPUT);

    if (digitalRead(PIN_PWR_BTN) == HIGH) {
        digitalWrite(PIN_BAT_LATCH, HIGH);
    } 
    else {
        digitalWrite(PIN_BAT_LATCH, LOW); 
        uint32_t hold_start = millis();
        while (millis() - hold_start < 2000) {
            if (digitalRead(PIN_PWR_BTN) == HIGH) {
                rtc_gpio_pullup_en((gpio_num_t)PIN_PWR_BTN);
                rtc_gpio_pulldown_dis((gpio_num_t)PIN_PWR_BTN);
                esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PWR_BTN, 0);
                esp_deep_sleep_start();
            }
            delay(10);
        }
        digitalWrite(PIN_BAT_LATCH, HIGH);
    }

    Serial.begin(115200);
    analogReadResolution(12);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    wakeUpHardware();

    if (!gfx->begin()) while(1);
    gfx->displayOn();
    gfx->invertDisplay(false);

    lv_init();
    buf1 = (lv_color_t *)heap_caps_malloc(screenWidth * screenHeight / 10 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, screenWidth * screenHeight / 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush; 
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // --- VISIBILITY FIX ---
    lv_obj_t * screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0); // Background BLACK
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    bat_label = lv_label_create(screen);
    lv_obj_clear_flag(bat_label, LV_OBJ_FLAG_HIDDEN); // Force visible
    lv_label_set_text(bat_label, "BAT: --%");
    lv_obj_set_style_text_color(bat_label, lv_color_hex(0xFF0000), 0); // RED
    lv_obj_align(bat_label, LV_ALIGN_TOP_MID, 0, 70); // Move down to clear round bezel

    main_label = lv_label_create(screen);
    lv_label_set_text(main_label, "BATTERY\nCHECK");
    lv_obj_set_style_text_color(main_label, lv_color_hex(0x00FF00), 0); 
    lv_obj_set_style_text_align(main_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, 0); 

    update_battery_status(); // Update immediately
}

void loop() {
    lv_timer_handler(); 
    static uint32_t last_bat_check = 0;
    if (millis() - last_bat_check > 5000) {
        last_bat_check = millis();
        update_battery_status();
    }
    delay(5);

    static uint32_t button_press_start = 0;
    static bool is_shutting_down = false;
    if (!is_shutting_down) {
        if (digitalRead(PIN_PWR_BTN) == LOW) {
            if (!ignore_initial_press) {
                if (button_press_start == 0) button_press_start = millis();
                else if (millis() - button_press_start > 2000) {
                    is_shutting_down = true;
                    execute_shutdown();
                }
            }
        } else {
            ignore_initial_press = false; 
            button_press_start = 0;
        }
    }
}