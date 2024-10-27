// museluxe.cpp - Implementation file for ESP Muse Luxe NeoPixel, Battery Management System (BMS) using IP5306, and ES8388 Audio Codec
// (c) 2024 RASPiAudio

#include "museluxe.h"
#include <driver/i2s.h>
#include <driver/gpio.h>
#include <esp_system.h>
#include <math.h>
#include <driver/ledc.h> // Added LEDC header for MCLK generation

MuseLuxe::MuseLuxe() : pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800) {}

void MuseLuxe::begin() {
    // Initialize the NeoPixel strip object (REQUIRED for the Muse Luxe's RGB LED)
    pixels.begin();
    pixels.show(); // Initialize all to 'off'

    // Initialize I2C for IP5306 and ES8388 communication
    Wire.begin(I2C_SDA, I2C_SCL);

    // Initialize button pins
    pinMode(BUTTON_PAUSE, INPUT_PULLUP);
    pinMode(BUTTON_VOL_MINUS, INPUT_PULLUP);
    pinMode(BUTTON_VOL_PLUS, INPUT_PULLUP);

    // Initialize Serial for debugging
    Serial.begin(115200);
    Serial.println("Muse Luxe initialized");

    // Initialize Audio
    initAudio();

    // Initially enable the amplifier
    setAmplifier(true); // Enable by default; change to 'false' if you want it disabled initially
}

uint8_t MuseLuxe::getBatteryPercentage() {
    return getBatteryLevel();
}

uint8_t MuseLuxe::getBatteryLevel() {
    Wire.beginTransmission(IP5306_I2C_ADDRESS);
    Wire.write(IP5306_REG_READ_4);
    // Explicitly cast address and size to uint8_t
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)IP5306_I2C_ADDRESS, (uint8_t)1)) {
        uint8_t level = Wire.read();
        return IP5306_LEDS2PCT(~level & 0x0F); // LED[0-4] State (inverted)
    }
    Serial.println("Error: Unable to read battery level from IP5306.");
    return 0;
}

// NeoPixel control methods
void MuseLuxe::setPixelColor(uint32_t color) {
    pixels.setPixelColor(0, color);
}

void MuseLuxe::showPixel() {
    pixels.show();
}

uint32_t MuseLuxe::getColor(uint8_t r, uint8_t g, uint8_t b) {
    return pixels.Color(r, g, b);
}

// IP5306 helper functions
int MuseLuxe::ip5306_get_reg(uint8_t reg) {
    Wire.beginTransmission(IP5306_I2C_ADDRESS);
    Wire.write(reg);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)IP5306_I2C_ADDRESS, (uint8_t)1)) {
        return Wire.read();
    }
    return -1;
}

int MuseLuxe::ip5306_set_reg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(IP5306_I2C_ADDRESS);
    Wire.write(reg);
    Wire.write(value);
    if (Wire.endTransmission(true) == 0) {
        return 0;
    }
    return -1;
}

uint8_t MuseLuxe::ip5306_get_bits(uint8_t reg, uint8_t index, uint8_t bits) {
    int value = ip5306_get_reg(reg);
    if (value < 0) {
        Serial.printf("ip5306_get_bits fail: 0x%02x\n", reg);
        return 0;
    }
    return (value >> index) & ((1 << bits) - 1);
}

void MuseLuxe::ip5306_set_bits(uint8_t reg, uint8_t index, uint8_t bits, uint8_t value) {
    uint8_t mask = (1 << bits) - 1;
    int v = ip5306_get_reg(reg);
    if (v < 0) {
        Serial.printf("ip5306_get_reg fail: 0x%02x\n", reg);
        return;
    }
    v &= ~(mask << index);
    v |= ((value & mask) << index);
    if (ip5306_set_reg(reg, v)) {
        Serial.printf("ip5306_set_bits fail: 0x%02x\n", reg);
    }
}

// Public methods to get IP5306 statuses
bool MuseLuxe::isCharging() {
    // IP5306_GetPowerSource() returns 1 if VIN (USB) is connected (charging), 0 if battery
    return IP5306_GetPowerSource();
}

uint8_t MuseLuxe::getVinCurrent() {
    return (IP5306_GetVinCurrent() * 100) + 50; // ImA = (v * 100) + 50
}

uint8_t MuseLuxe::getVoltagePressure() {
    return IP5306_GetVoltagePressure() * 14; // Voltage Pressure = bits * 14mV
}

// Audio initialization
void MuseLuxe::initAudio() {
    // Initialize ES8388 codec
    ES8388_Init();

    // Initialize I2S
    I2S_Init();
}

// Amplifier control
void MuseLuxe::setAmplifier(bool enable) {
    gpio_set_level(PA_PIN, enable ? 1 : 0);
    Serial.printf("Amplifier %s\n", enable ? "Enabled" : "Disabled");
}

void MuseLuxe::ES8388_Init() {
  // reset
  ES8388_Write_Reg(0, 0x80);
  ES8388_Write_Reg(0, 0x00);
  // mute
  ES8388_Write_Reg(25, 0x04);
  ES8388_Write_Reg(1, 0x50);
  //powerup
  ES8388_Write_Reg(2, 0x00);
  // slave mode
  ES8388_Write_Reg(8, 0x00);
  // DAC powerdown
  ES8388_Write_Reg(4, 0xC0);
  // vmidsel/500k ADC/DAC idem
  ES8388_Write_Reg(0, 0x12);

  ES8388_Write_Reg(1, 0x00);
  // i2s 16 bits
  ES8388_Write_Reg(23, 0x18);
  // sample freq 256
  ES8388_Write_Reg(24, 0x02);
  // LIN2/RIN2 for mixer
  ES8388_Write_Reg(38, 0x09);
  // left DAC to left mixer
  ES8388_Write_Reg(39, 0x90);
  // right DAC to right mixer
  ES8388_Write_Reg(42, 0x90);
  // DACLRC ADCLRC idem
  ES8388_Write_Reg(43, 0x80);
  ES8388_Write_Reg(45, 0x00);
  // DAC volume max
  ES8388_Write_Reg(27, 0x00);
  ES8388_Write_Reg(26, 0x00);

  ES8388_Write_Reg(2 , 0xF0);
  ES8388_Write_Reg(2 , 0x00);
  ES8388_Write_Reg(29, 0x1C);
  // DAC power-up LOUT1/ROUT1 enabled
  ES8388_Write_Reg(4, 0x30);
  // unmute
  ES8388_Write_Reg(25, 0x00);
}

void MuseLuxe::I2S_Init() {
    // Configure I2S driver
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_config.sample_rate = 44100;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_I2S;
    i2s_config.intr_alloc_flags = 0; // Default interrupt priority
    i2s_config.dma_buf_count = 8;
    i2s_config.dma_buf_len = 64;
    i2s_config.use_apll = false;
    i2s_config.tx_desc_auto_clear = true; // Avoid noise in case of data unavailability

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = I2S_BCLK;
    pin_config.ws_io_num = I2S_LRC;
    pin_config.data_out_num = I2S_DOUT;
    pin_config.data_in_num = I2S_DIN; // Not used

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void MuseLuxe::playSineWave(uint16_t frequency, uint16_t duration_ms) {
    const int sample_rate = 44100;
    const int samples = (sample_rate * duration_ms) / 1000;
    const double two_pi = 2.0 * M_PI;
    const double increment = two_pi * frequency / sample_rate;
    const int amplitude = 30000; // Reduced amplitude to prevent clipping
    size_t bytes_written = 0;

    for (int i = 0; i < samples; ++i) {
        double sample = sin(increment * i);
        int16_t sample_val = (int16_t)(sample * amplitude);

        // Stereo sample: duplicate for both left and right channels
        int16_t samples_data[2] = {sample_val, sample_val};

        i2s_write(I2S_NUM_0, (const char *)samples_data, sizeof(samples_data), &bytes_written, portMAX_DELAY);
    }
}

// Implement ES8388 Write and Read using Wire.h
void MuseLuxe::ES8388_Write_Reg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8388_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

uint8_t MuseLuxe::ES8388_Read_Reg(uint8_t reg) {
    Wire.beginTransmission(ES8388_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false); // Send restart
    Wire.requestFrom((uint8_t)ES8388_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    return 0;
}

void MuseLuxe::setVolume(uint8_t volume) {
    if (volume > 100) volume = 100; // Cap volume at 100%
    currentVolume = volume;

    // Convert percentage to codec volume register value
    uint8_t codecVolume = 255 - (volume * 255) / 100; // Invert if necessary

    // Set the DAC volume registers (26 and 27 are Left and Right DAC volume)
    ES8388_Write_Reg(26, codecVolume); // Left DAC
    ES8388_Write_Reg(27, codecVolume); // Right DAC

    Serial.printf("Codec volume set to 0x%02X\n", codecVolume);
}

uint8_t MuseLuxe::getVolume() {
    return currentVolume;
}