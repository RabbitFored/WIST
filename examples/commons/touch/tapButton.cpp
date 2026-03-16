#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>

extern "C" {
    #include <lvgl.h>
}

// --- W.I.S.T. HARDWARE MAP (1.46B) ---
#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10
#define PIN_TP_INT  4     
#define TFT_CS      21
#define TFT_SCK     40
#define TFT_D0      46 
#define TFT_D1      45
#define TFT_D2      42
#define TFT_D3      41
#define PIN_LCD_BL  5

#define TCA9554_ADDR 0x20 
#define SPD2010_ADDR 0x53  // The True TDDI Address
#define EXIO_TP_RST  (1 << 1) // P1 on Expander
#define EXIO_LCD_RST (1 << 2) // P2 on Expander

// --- OBJECTS ---
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);
Arduino_GFX *gfx = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);

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

// --- SPD2010 TDDI RAW HELPERS ---
bool spd2010_write(uint16_t reg, uint8_t *data, uint8_t len) {
    Wire.beginTransmission(SPD2010_ADDR);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    for (int i = 0; i < len; i++) Wire.write(data[i]);
    return (Wire.endTransmission(true) == 0); // True forces a clean STOP, preventing 259 errors
}

bool spd2010_read(uint16_t reg, uint8_t *data, uint8_t len) {
    Wire.beginTransmission(SPD2010_ADDR);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission(true) != 0) return false;
    
    Wire.requestFrom((uint16_t)SPD2010_ADDR, (uint8_t)len);
    for (uint8_t i = 0; i < len; i++) {
        if (Wire.available()) data[i] = Wire.read();
    }
    return true;
}

// --- SILICON VERIFICATION ---
void test_spd2010_firmware() {
    uint8_t sample_data[18];
    Serial.println("\nQuerying SPD2010 Firmware at 0x53...");
    
    if (spd2010_read(0x2600, sample_data, 18)) {
        uint32_t Dummy = ((sample_data[0] << 24) | (sample_data[1] << 16) | (sample_data[3] << 8) | (sample_data[0]));
        uint16_t DVer = ((sample_data[5] << 8) | (sample_data[4]));
        Serial.printf("SUCCESS! TDDI Awake. -> Dummy[%d], DVer[%d]\n", Dummy, DVer);
    } else {
        Serial.println("FAILED! 0x53 is still unresponsive.");
    }
}

// --- THE TDDI STATE MACHINE ---
bool spd2010_state_machine(int16_t &x, int16_t &y) {
    uint8_t status_data[4];
    if (!spd2010_read(0x2000, status_data, 4)) return false;

    bool pt_exist = (status_data[0] & 0x01);
    bool tic_in_bios = ((status_data[1] & 0x40) >> 6);
    bool tic_in_cpu = ((status_data[1] & 0x20) >> 5);
    bool cpu_run = ((status_data[1] & 0x08) >> 3);
    uint16_t read_len = (status_data[3] << 8 | status_data[2]);

    uint8_t cmd_clear_int[2] = {0x01, 0x00};

    if (tic_in_bios) {
        spd2010_write(0x0200, cmd_clear_int, 2); 
        spd2010_write(0x0400, (uint8_t[]){0x01, 0x00}, 2); 
    } 
    else if (tic_in_cpu) {
        spd2010_write(0x5000, (uint8_t[]){0x00, 0x00}, 2); 
        spd2010_write(0x4600, (uint8_t[]){0x00, 0x00}, 2); 
        spd2010_write(0x0200, cmd_clear_int, 2); 
    } 
    else if (cpu_run && read_len == 0) {
        spd2010_write(0x0200, cmd_clear_int, 2); 
    } 
    else if (pt_exist) {
        if (read_len > 64) read_len = 64; 
        uint8_t hdp_data[64]; 
        
        if (spd2010_read(0x0003, hdp_data, read_len)) {
            bool valid_touch = false;
            if (hdp_data[4] <= 0x0A) { 
                x = (((hdp_data[7] & 0xF0) << 4) | hdp_data[5]);
                y = (((hdp_data[7] & 0x0F) << 8) | hdp_data[6]);
                valid_touch = true;
            }
            uint8_t hdp_status[8];
            if (spd2010_read(0xFC02, hdp_status, 8)) {
                if (hdp_status[5] == 0x82) {
                    spd2010_write(0x0200, cmd_clear_int, 2); 
                }
            }
            return valid_touch;
        }
    }
    return false;
}

// --- LVGL TOUCH BRIDGE ---
void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    if (digitalRead(PIN_TP_INT) == LOW) {
        int16_t touch_x, touch_y;
        if (spd2010_state_machine(touch_x, touch_y)) {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = touch_x;
            data->point.y = touch_y;
            Serial.printf("TDDI TOUCH -> X: %d, Y: %d\n", touch_x, touch_y);
            return;
        }
    }
    data->state = LV_INDEV_STATE_REL;
}

// --- UI LOGIC ---
static void btn_event_cb(lv_event_t * e) {
    static int color_idx = 0;
    lv_color_t colors[] = {lv_color_hex(0xFF0000), lv_color_hex(0x00FF00), lv_color_hex(0x0000FF)};
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_set_style_bg_color(btn, colors[color_idx], 0);
    color_idx = (color_idx + 1) % 3;
}

// --- HARDWARE WAKE (FIXED) ---
void wakeUpHardware() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000); 
    
    // Set all TCA9554 pins as outputs
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); 
    Wire.write(0x00); 
    Wire.endTransmission();
    
    // THE FIX: Set ALL pins HIGH (This disables SD Card and turns ON touch power)
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); 
    Wire.write(0xFF); 
    Wire.endTransmission();
    delay(50);
    
    // Pull ONLY the Touch and LCD Reset pins LOW
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); 
    Wire.write(0xFF & ~(EXIO_LCD_RST | EXIO_TP_RST)); 
    Wire.endTransmission();
    delay(50);
    
    // Push Touch and LCD Reset HIGH to boot them up normally
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x01); 
    Wire.write(0xFF); 
    Wire.endTransmission();
    delay(150); 
}

// --- I2C SCANNER ---
void scanBus() {
    Serial.println("\n--- I2C SCAN ---");
    for(byte address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Found: 0x%02X\n", address);
        }
    }
    Serial.println("----------------");
}

void setup() {
    Serial.begin(115200);
    delay(3000); // Wait for COM port to open
    Serial.println("\n\n=== W.I.S.T. SYSTEM BOOT ===");

    // 1. Hardware Wake
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    pinMode(PIN_TP_INT, INPUT_PULLUP);
    wakeUpHardware();
    
    // 2. Scan the bus to verify 0x53 is now alive
    scanBus();

    // 3. Start Display 
    if (!gfx->begin()) while(1);
    gfx->displayOn();
    gfx->invertDisplay(false);

    // 4. Test Silicon!
    test_spd2010_firmware();

    // 5. Force TDDI Boot Sequence
    Serial.println("Forcing TDDI CPU Boot...");
    int16_t dummy_x, dummy_y;
    for (int i = 0; i < 15; i++) {
        spd2010_state_machine(dummy_x, dummy_y); 
        delay(20);
    }
    Serial.println("TDDI CPU Boot Complete.");

    // 6. Initialize LVGL
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

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // 7. UI
    lv_obj_t * btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn, 250, 150);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "TAP HERE");
    lv_obj_center(label);
}

void loop() {
    lv_timer_handler(); 
    delay(5);
}