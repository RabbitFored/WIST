#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <vector>

// --- W.I.S.T. HARDWARE MAP ---
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
Arduino_DataBus *bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_D0, TFT_D1, TFT_D2, TFT_D3);
Arduino_GFX *gfx = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);

static const uint32_t screenWidth  = 412;
static const uint32_t screenHeight = 412;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;

// --- UI AND BLE GLOBALS ---
lv_obj_t * ble_list;
lv_obj_t * status_label;

struct BleDevice {
    String name;
    int rssi;
    String address;
};

std::vector<BleDevice> foundDevices;
bool scanComplete = false;
bool isScanning = false;
BLEScan* pBLEScan;

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

// --- BLE CALLBACKS ---
// This runs in the background every time a device pings the watch
// --- BLE CALLBACKS ---
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        BleDevice dev;
        
        // 1. RAW DIAGNOSTIC: Print everything to the Serial Monitor immediately
        Serial.print("RADAR PING -> MAC: ");
        Serial.print(advertisedDevice.getAddress().toString().c_str());
        Serial.print(" | RSSI: ");
        Serial.println(advertisedDevice.getRSSI());

        // 2. Grab name if it exists, otherwise use the MAC address
        if(advertisedDevice.haveName()) {
            dev.name = advertisedDevice.getName().c_str();
        } else {
            dev.name = advertisedDevice.getAddress().toString().c_str(); 
        }
        
        dev.rssi = advertisedDevice.getRSSI();
        dev.address = advertisedDevice.getAddress().toString().c_str();

        // 3. NO FILTERS: Add literally every signal to the UI list
        bool exists = false;
        for(auto &d : foundDevices) {
            if(d.address == dev.address) {
                exists = true;
                d.rssi = dev.rssi; // Update signal strength
                break;
            }
        }
        if(!exists) {
            foundDevices.push_back(dev);
        }
    }
};
// This triggers when the 3-second background scan finishes
void scanCompleteCB(BLEScanResults results) {
    scanComplete = true;
    isScanning = false;
}

// --- UI UPDATER ---
void update_ui_list() {
    lv_obj_clean(ble_list); // Wipe the old list
    
    if (foundDevices.size() == 0) {
        lv_list_add_text(ble_list, "No devices found");
        return;
    }

    // Populate the scrolling list with the found devices
    for(auto &dev : foundDevices) {
        String item_text = dev.name + " (" + String(dev.rssi) + "dBm)";
        lv_obj_t * btn = lv_list_add_btn(ble_list, LV_SYMBOL_BLUETOOTH, item_text.c_str());
        
        // Style the list buttons to look tactical
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
    }
}

void setup() {
    Serial.begin(115200);

    // 1. Display Init
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

    // 2. BLE Init
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true); // Active scan uses more power, but gets names faster
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99); 

    // 3. UI Layout
    lv_obj_t * screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0); 

    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "INITIALIZING RADAR...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00FF00), 0); 
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 40); 

    // Create the scrolling list container
    ble_list = lv_list_create(screen);
    lv_obj_set_size(ble_list, 280, 240); // Shrunk slightly to fit inside the round bezel
    lv_obj_align(ble_list, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(ble_list, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_color(ble_list, lv_color_hex(0x00FF00), 0);
}

void loop() {
    // Keep the UI running smoothly
    lv_timer_handler(); 
    delay(5);

    static uint32_t last_scan_time = 0;

    // 1. THE UI GATE: If a scan just finished, draw the data FIRST.
    if (scanComplete) {
        scanComplete = false;
        lv_label_set_text(status_label, "RADAR COMPLETE");
        
        // Draw the devices to the screen
        update_ui_list(); 
        
        // Reset the cooldown clock NOW, so we wait 2 full seconds before clearing
        last_scan_time = millis(); 
    }

    // 2. THE SCAN GATE: Wait for the 2-second cooldown AFTER the UI updates
    if (!isScanning && !scanComplete && (millis() - last_scan_time > 2000)) {
        lv_label_set_text(status_label, "SCANNING... " LV_SYMBOL_BLUETOOTH);
        
        // Now it is safe to wipe the old list
        foundDevices.clear(); 
        pBLEScan->clearResults(); 
        isScanning = true;
        
        // Start a 3-second NON-BLOCKING scan
        pBLEScan->start(3, scanCompleteCB, false); 
    }
}