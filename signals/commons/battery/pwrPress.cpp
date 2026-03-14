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

static const uint32_t screenWidth  = 412;
static const uint32_t screenHeight = 412;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
lv_obj_t * main_label; 

// A safety flag so the watch doesn't immediately turn off after booting
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

// --- SHUTDOWN PROTOCOL ---
void execute_shutdown() {
    lv_label_set_text(main_label, "SYSTEM\nOFF");
    lv_obj_set_style_text_color(main_label, lv_color_hex(0xFF0000), 0); 
    lv_refr_now(NULL); 
    delay(800); 

    digitalWrite(PIN_LCD_BL, LOW);

    // Lock the pull-up resistor ON before going to sleep so the button works on USB
    rtc_gpio_pullup_en((gpio_num_t)PIN_PWR_BTN);
    rtc_gpio_pulldown_dis((gpio_num_t)PIN_PWR_BTN);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PWR_BTN, 0);

    digitalWrite(PIN_BAT_LATCH, LOW); 
    esp_deep_sleep_start();           
}

void setup() {
    pinMode(PIN_PWR_BTN, INPUT_PULLUP);
    pinMode(PIN_BAT_LATCH, OUTPUT);

    // --- THE SMART BOOT GATEKEEPER ---
    // If the button is NOT pressed at boot, it means we just flashed code or plugged in USB.
    // Skip the 2-second hold and turn ON instantly!
    if (digitalRead(PIN_PWR_BTN) == HIGH) {
        digitalWrite(PIN_BAT_LATCH, HIGH);
    } 
    // If the button IS pressed, the user is turning it on. Enforce the 2-second rule.
    else {
        digitalWrite(PIN_BAT_LATCH, LOW); // Keep deadbolt open
        bool boot_approved = true;
        uint32_t hold_start = millis();

        // Check continuously for 2 seconds
        while (millis() - hold_start < 2000) {
            if (digitalRead(PIN_PWR_BTN) == HIGH) {
                // The user let go of the button early!
                boot_approved = false;
                break;
            }
            delay(10);
        }

        if (!boot_approved) {
            // Failed the 2-second hold. Go back to sleep.
            rtc_gpio_pullup_en((gpio_num_t)PIN_PWR_BTN);
            rtc_gpio_pulldown_dis((gpio_num_t)PIN_PWR_BTN);
            esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_PWR_BTN, 0);
            esp_deep_sleep_start();
        }

        // Passed the 2-second hold! Latch power.
        digitalWrite(PIN_BAT_LATCH, HIGH);
    }

    // --- STARTUP W.I.S.T. SUBSYSTEMS ---
    Serial.begin(115200);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    wakeUpHardware();

    if (!gfx->begin()) {
        Serial.println("GFX Init Failed!");
        while(1);
    }
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

    main_label = lv_label_create(lv_scr_act());
    lv_label_set_text(main_label, "W.I.S.T.\nONLINE");
    lv_obj_set_style_text_color(main_label, lv_color_hex(0x00FF00), 0); 
    lv_obj_set_style_text_align(main_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, 0); 
}

void loop() {
    lv_timer_handler(); 
    delay(5);

    // --- BUTTON HOLD LOGIC ---
    static uint32_t button_press_start = 0;
    static bool is_shutting_down = false;

    if (!is_shutting_down) {
        if (digitalRead(PIN_PWR_BTN) == LOW) {
            
            // Wait until the user finishes their initial "Turn On" press
            if (ignore_initial_press) return; 

            if (button_press_start == 0) {
                button_press_start = millis();
            } 
            else if (millis() - button_press_start > 2000) {
                is_shutting_down = true;
                execute_shutdown();
            }
        } else {
            // The button is released. We can now safely arm the shutdown timer!
            ignore_initial_press = false; 
            button_press_start = 0;
        }
    }
}