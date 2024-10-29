// museluxe.cpp - Implementation file for ESP Muse Luxe NeoPixel, Battery Management System (BMS) using IP5306, and ES8388 Audio Codec
// (c) 2024 RASPiAudio

#include "museluxe.h"
#include <driver/gpio.h>
#include <esp_system.h>



MuseLuxe::MuseLuxe() : pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800) {}

void MuseLuxe::begin() {
    // Initialize the NeoPixel strip object (REQUIRED for the Muse Luxe's RGB LED)
    pixels.begin();
    pixels.show(); // Initialize all to 'off'

    // Initialize I2C for IP5306 and ES8388 communication
    Wire.begin(IIC_DATA, IIC_CLK);

    // Initialize button pins
    pinMode(BUTTON_PAUSE, INPUT_PULLUP);
    pinMode(BUTTON_VOL_MINUS, INPUT_PULLUP);
    pinMode(BUTTON_VOL_PLUS, INPUT_PULLUP);

    // Initialize Serial for debugging
    Serial.begin(115200);
    Serial.println("Muse Luxe initialized");

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
        Serial.printf("ip5306_set_reg fail: 0x%02x\n", reg);
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
