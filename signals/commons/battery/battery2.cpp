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
#define PIN_BAT_ADC 8     

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
lv_obj_t * raw_volt_label; // NEW: Label for raw telemetry

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

// --- ENHANCED BATTERY MONITORING ---
const float BATTERY_CURVE[][2] = {
    {4.20, 100}, {4.15, 95},  {4.11, 90},  {4.08, 85},  {4.02, 80},
    {3.98, 75},  {3.95, 70},  {3.91, 65},  {3.87, 60},  {3.85, 55},
    {3.84, 50},  {3.82, 45},  {3.80, 40},  {3.79, 35},  {3.77, 30},
    {3.75, 25},  {3.73, 20},  {3.71, 15},  {3.69, 10},  {3.61, 5},
    {3.27, 0}
};
const int CURVE_POINTS = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);

int voltage_to_percentage(float voltage) {
    if (voltage >= BATTERY_CURVE[0][0]) return 100;
    if (voltage <= BATTERY_CURVE[CURVE_POINTS-1][0]) return 0;
    
    for (int i = 0; i < CURVE_POINTS - 1; i++) {
        if (voltage >= BATTERY_CURVE[i+1][0]) {
            float v_high = BATTERY_CURVE[i][0];
            float v_low = BATTERY_CURVE[i+1][0];
            float p_high = BATTERY_CURVE[i][1];
            float p_low = BATTERY_CURVE[i+1][1];
            
            float ratio = (voltage - v_low) / (v_high - v_low);
            return (int)(p_low + ratio * (p_high - p_low));
        }
    }
    return 0;
}

// --- DYNAMIC SMARTPHONE CHARGE TRACKING ---
void update_battery_status() {
    // 1. MEDIAN FILTER: Outlier Rejection for absolute stability
    long readings[20];
    for (int i = 0; i < 20; i++) {
        readings[i] = analogReadMilliVolts(PIN_BAT_ADC);
        delay(2);
    }
    
    // Bubble sort the readings to isolate noise
    for(int i = 0; i < 19; i++) {
        for(int j = i + 1; j < 20; j++) {
            if(readings[i] > readings[j]) {
                long temp = readings[i];
                readings[i] = readings[j];
                readings[j] = temp;
            }
        }
    }
    
    // Average only the clean middle 10 readings
    float avg_mv = 0;
    for(int i = 5; i < 15; i++) {
        avg_mv += readings[i];
    }
    avg_mv /= 10.0; // Float division prevents truncation
    
    // Calculate final voltage
    float voltage = (avg_mv * 3.0 / 1000.0) / 0.990476;

    // Static State Tracking
    static float last_voltage = -1.0;
    static bool is_charging = false;
    static int displayed_percent = -1;
    static uint32_t last_increment_time = 0;

    int raw_percent = voltage_to_percentage(voltage);

    // 2. First Boot Initialization
    if (last_voltage < 0) {
        last_voltage = voltage;
        displayed_percent = raw_percent;
        if (voltage > 4.15) is_charging = true; 
    }

    // 3. Asymmetric Jump Detectors
    if (voltage > last_voltage + 0.05) {
        is_charging = true;
        last_voltage = voltage; 
    }
    else if (voltage < last_voltage - 0.08) {
        is_charging = false;
        last_voltage = voltage; 
    } 
    else {
        last_voltage = (last_voltage * 0.98) + (voltage * 0.02); 
    }

    if (voltage >= 4.22) is_charging = true;

    // 4. The Slew Filter
    if (is_charging) {
        if (raw_percent > displayed_percent && displayed_percent < 100) {
            if (millis() - last_increment_time > 5000) {
                displayed_percent++;
                last_increment_time = millis();
            }
        }
        if (voltage >= 4.20) displayed_percent = 100;
    } 
    else {
        if (raw_percent < displayed_percent) {
            displayed_percent = raw_percent;
        }
    }

    if (displayed_percent > 100) displayed_percent = 100;
    if (displayed_percent < 0) displayed_percent = 0;

    // 5. Update Main UI
    char bat_str[64];
    if (is_charging) {
        sprintf(bat_str, "CHG: %d%% ⚡", displayed_percent);
        lv_obj_set_style_text_color(bat_label, lv_color_hex(0xFFFF00), 0); // Yellow
    } else {
        sprintf(bat_str, "BAT: %d%%", displayed_percent);
        if (displayed_percent <= 15) {
            lv_obj_set_style_text_color(bat_label, lv_color_hex(0xFF0000), 0); // Red
        } else {
            lv_obj_set_style_text_color(bat_label, lv_color_hex(0x00FF00), 0); // Green
        }
    }
    lv_label_set_text(bat_label, bat_str);

    // 6. Update Raw Telemetry UI (The exact calculated math)
    char raw_str[64];
    sprintf(raw_str, "RAW: %.2fV | ADC: %d mV", voltage, (int)avg_mv);
    lv_label_set_text(raw_volt_label, raw_str);
}

// --- SHUTDOWN PROTOCOL ---
void execute_shutdown() {
    lv_obj_add_flag(bat_label, LV_OBJ_FLAG_HIDDEN); 
    lv_obj_add_flag(raw_volt_label, LV_OBJ_FLAG_HIDDEN); 
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

    // Smart Boot Gatekeeper
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
    digitalWrite(PIN_LCD_BL, HIGH); // Full brightness for now
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

    // --- VISIBILITY & UI ---
    lv_obj_t * screen = lv_scr_act();
    // Corrected to absolute Black to fix the snowblind issue
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0); 
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // Main Battery Percentage Label
    bat_label = lv_label_create(screen);
    lv_label_set_text(bat_label, "BAT: --%");
    lv_obj_set_style_text_color(bat_label, lv_color_hex(0xFFFFFF), 0); 
    lv_obj_align(bat_label, LV_ALIGN_TOP_MID, 0, 70); 

    // Secondary Raw Voltage Label
    raw_volt_label = lv_label_create(screen);
    lv_label_set_text(raw_volt_label, "RAW: --V");
    lv_obj_set_style_text_color(raw_volt_label, lv_color_hex(0x888888), 0); // Tactical Grey
    lv_obj_align(raw_volt_label, LV_ALIGN_TOP_MID, 0, 100); // Positioned underneath main bat

    // Center Display Label
    main_label = lv_label_create(screen);
    lv_label_set_text(main_label, "BATTERY\nCHECK");
    lv_obj_set_style_text_color(main_label, lv_color_hex(0x00FF00), 0); 
    lv_obj_set_style_text_align(main_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(main_label, LV_ALIGN_CENTER, 0, 0); 

    update_battery_status(); // Initial update
}

void loop() {
    lv_timer_handler(); 
    
    // Battery monitoring loop
    static uint32_t last_bat_check = 0;
    uint32_t bat_interval = 5000; 
    
    if (millis() - last_bat_check > bat_interval) {
        last_bat_check = millis();
        update_battery_status();
    }
    
    delay(5);

    // Power button debounce and shutdown logic
    static uint32_t button_press_start = 0;
    static bool is_shutting_down = false;
    static uint32_t last_button_state = HIGH;
    static uint32_t button_debounce_time = 0;
    
    uint32_t current_button_state = digitalRead(PIN_PWR_BTN);
    
    if (current_button_state != last_button_state) {
        button_debounce_time = millis();
    }
    
    if (!is_shutting_down && (millis() - button_debounce_time) > 50) { 
        if (current_button_state == LOW) {
            if (!ignore_initial_press) {
                if (button_press_start == 0) {
                    button_press_start = millis();
                }
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
    
    last_button_state = current_button_state;
}