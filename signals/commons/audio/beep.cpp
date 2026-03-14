#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>
#include <Wire.h>

// --- HARDWARE ANATOMY ---
#define I2S_LRCK 38
#define I2S_BCK  48
#define I2S_DIN  47

// --- AUDIO SETTINGS ---
#define SAMPLE_RATE 44100
#define VOLUME_GAIN  0.6  // <--- UPDATED: 60% Power (Careful, Doctor)

// --- VARIABLES FOR SYNTHESIS ---
float currentFreq = 100.0; // Start Low
float stepSize = 10.0;     // How fast to rise
int   direction = 1;       // 1 = Up, -1 = Down

void enableAmplifier() {
    Wire.begin(11, 10);
    Wire.beginTransmission(0x20); // TCA9554
    Wire.write(0x03); Wire.write(0x00); // Output
    Wire.endTransmission();
    
    Wire.beginTransmission(0x20);
    Wire.write(0x01); Wire.write(0xFF); // High
    Wire.endTransmission();
}

void setupI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK,
        .ws_io_num = I2S_LRCK,
        .data_out_num = I2S_DIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void setup() {
    Serial.begin(115200);
    enableAmplifier();
    setupI2S();
    Serial.println("INITIATING REPULSOR CHARGE SEQUENCE...");
}

void loop() {
    size_t bytes_written;
    int16_t samples[256]; // Buffer
    
    // We generate a short chunk of audio at the current frequency
    static float phase = 0;
    float phase_increment = (2 * PI * currentFreq) / SAMPLE_RATE;

    for (int i = 0; i < 256; i += 2) {
        int16_t value = (int16_t)(sin(phase) * 32767 * VOLUME_GAIN);
        samples[i] = value;     
        samples[i+1] = value;   
        
        phase += phase_increment;
        if (phase >= 2 * PI) phase -= 2 * PI;
    }

    // Write to I2S (Play the sound)
    i2s_write(I2S_NUM_0, samples, sizeof(samples), &bytes_written, portMAX_DELAY);

    // --- MODIFY FREQUENCY (The "Complex" Part) ---
    // Every loop cycle, we increase the pitch to simulate charging
    currentFreq += stepSize;
    
    // If we hit the top, reset (or bounce)
    if (currentFreq > 1500.0) {
        currentFreq = 100.0; // Reset to low (Pulse effect)
        // Alternative: direction = -1; (Siren effect)
    }
}