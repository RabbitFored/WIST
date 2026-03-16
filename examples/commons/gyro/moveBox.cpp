#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <math.h> // Required for circular boundary math

// --- W.I.S.T. HARDWARE MAP ---
#define PIN_I2C_SDA 11
#define PIN_I2C_SCL 10
#define QMI8658_ADDR 0x6B

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

// --- GAME & PHYSICS VARIABLES ---
lv_obj_t * player_box;
const int box_size = 40; 
const int center_x = 206;
const int center_y = 206;
// Maximum allowed distance from the center. 
// 206 (screen radius) - 20 (half the box size) = 186
const int max_radius = 186; 

// Track the EXACT center of the box for trigonometry
float box_cx = 206.0;
float box_cy = 206.0;

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

void init_IMU() {
    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x02); 
    Wire.write(0x60); 
    Wire.endTransmission();

    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x03); 
    Wire.write(0x23); 
    Wire.endTransmission();

    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x08); 
    Wire.write(0x03); 
    Wire.endTransmission();
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH); 
    wakeUpHardware();
    init_IMU();

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

    // --- GAME UI SETUP ---
    lv_obj_t * screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0); 

    // Create the Player Box
    player_box = lv_obj_create(screen);
    lv_obj_set_size(player_box, box_size, box_size);
    lv_obj_set_style_bg_color(player_box, lv_color_hex(0x00FF00), 0); 
    lv_obj_set_style_border_width(player_box, 0, 0);
    lv_obj_set_style_radius(player_box, 8, 0); 
    
    // Convert Center Coords to Top-Left Coords for initial LVGL placement
    lv_obj_set_pos(player_box, center_x - (box_size / 2), center_y - (box_size / 2));
}

void loop() {
    lv_timer_handler(); 

    Wire.beginTransmission(QMI8658_ADDR);
    Wire.write(0x35); 
    Wire.endTransmission();
    Wire.requestFrom(QMI8658_ADDR, 6); 

    if (Wire.available() == 6) {
        int16_t ax = Wire.read() | (Wire.read() << 8);
        int16_t ay = Wire.read() | (Wire.read() << 8);
        int16_t az = Wire.read() | (Wire.read() << 8); 

        // 1. Calculate Velocity
        float velocity_x = 0;
        float velocity_y = 0;
        int deadzone = 200; // Increased sensitivity
        
        if (abs(ax) > deadzone) {
            velocity_x = (ax > 0 ? (ax - deadzone) : (ax + deadzone)) / 400.0;
        }
        if (abs(ay) > deadzone) {
            velocity_y = (ay > 0 ? (ay - deadzone) : (ay + deadzone)) / 400.0;
        }

        // 2. Apply Velocity to proposed center coordinates
        box_cx -= velocity_x; 
        box_cy += velocity_y;

        // 3. CIRCULAR COLLISION DETECTION
        // Calculate current distance from the absolute center of the display
        float dx = box_cx - center_x;
        float dy = box_cy - center_y;
        float distance = sqrt((dx * dx) + (dy * dy));

        // If the box tries to leave the circle, lock it exactly on the edge
        if (distance > max_radius) {
            box_cx = center_x + ((dx / distance) * max_radius);
            box_cy = center_y + ((dy / distance) * max_radius);
        }

        // 4. Update Screen (Converting center coords back to top-left for LVGL)
        int draw_x = (int)(box_cx - (box_size / 2));
        int draw_y = (int)(box_cy - (box_size / 2));
        lv_obj_set_pos(player_box, draw_x, draw_y);
    }
    
    delay(16); 
}