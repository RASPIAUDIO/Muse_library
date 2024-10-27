// museluxe.h - Header file for ESP Muse Luxe NeoPixel, Battery Management System (BMS) using IP5306, and ES8388 Audio Codec
// (c) 2024 RASPiAudio

#ifndef MUSELUXE_H
#define MUSELUXE_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <driver/ledc.h> // Added LEDC header for MCLK generation

// Pin Definitions
#define NEOPIXEL_PIN 22        // Pin for the RGB LED on Muse Luxe
#define NUMPIXELS 1            // Muse Luxe has 1 RGB LED
#define I2C_SDA 18             // I2C SDA Pin
#define I2C_SCL 23             // I2C SCL Pin
// Buttons definition
#define BUTTON_PAUSE GPIO_NUM_12  // Pause/Play button
#define BUTTON_VOL_MINUS GPIO_NUM_32 // Volume - button
#define BUTTON_VOL_PLUS GPIO_NUM_19  // Volume + button



// Audio Codec ES8388 I2C Address
#define ES8388_ADDR 0x10

// I2S Pins
#define I2S_BCLK 5    // Bit Clock
#define I2S_LRC 25    // Left/Right Clock (Word Select)
#define I2S_DOUT 26   // Data Out
#define I2S_DIN -1    // Data In (not used in this example)

// Battery Level Thresholds
#define BATTERY_FULL 75
#define BATTERY_LOW  25
#define BATTERY_PIN 34 //for legacy ADC pin (not the I2C bms mode)

// IP5306 I2C Address and Registers
#define IP5306_I2C_ADDRESS 0x75
#define IP5306_REG_SYS_0    0x00
#define IP5306_REG_SYS_1    0x01
#define IP5306_REG_SYS_2    0x02
#define IP5306_REG_CHG_0    0x20
#define IP5306_REG_CHG_1    0x21
#define IP5306_REG_CHG_2    0x22
#define IP5306_REG_CHG_3    0x23
#define IP5306_REG_CHG_4    0x24
#define IP5306_REG_READ_0   0x70
#define IP5306_REG_READ_1   0x71
#define IP5306_REG_READ_2   0x72
#define IP5306_REG_READ_3   0x77
#define IP5306_REG_READ_4   0x78

// Macros to read specific bits from IP5306 registers
#define IP5306_GetKeyOffEnabled()               ip5306_get_bits(IP5306_REG_SYS_0, 0, 1)
#define IP5306_GetBoostOutputEnabled()          ip5306_get_bits(IP5306_REG_SYS_0, 1, 1)
#define IP5306_GetPowerOnLoadEnabled()          ip5306_get_bits(IP5306_REG_SYS_0, 2, 1)
#define IP5306_GetChargerEnabled()              ip5306_get_bits(IP5306_REG_SYS_0, 4, 1)
#define IP5306_GetBoostEnabled()                ip5306_get_bits(IP5306_REG_SYS_0, 5, 1)
#define IP5306_GetLowBatShutdownEnable()        ip5306_get_bits(IP5306_REG_SYS_1, 0, 1)
#define IP5306_GetBoostAfterVin()               ip5306_get_bits(IP5306_REG_SYS_1, 2, 1)
#define IP5306_GetShortPressBoostSwitchEnable() ip5306_get_bits(IP5306_REG_SYS_1, 5, 1)
#define IP5306_GetFlashlightClicks()            ip5306_get_bits(IP5306_REG_SYS_1, 6, 1)
#define IP5306_GetBoostOffClicks()              ip5306_get_bits(IP5306_REG_SYS_1, 7, 1)
#define IP5306_GetLightLoadShutdownTime()       ip5306_get_bits(IP5306_REG_SYS_2, 2, 2)
#define IP5306_GetLongPressTime()               ip5306_get_bits(IP5306_REG_SYS_2, 4, 1)
#define IP5306_GetChargingFullStopVoltage()     ip5306_get_bits(IP5306_REG_CHG_0, 0, 2)
#define IP5306_GetChargeUnderVoltageLoop()      ip5306_get_bits(IP5306_REG_CHG_1, 2, 3)
#define IP5306_GetEndChargeCurrentDetection()  ip5306_get_bits(IP5306_REG_CHG_1, 6, 2)
#define IP5306_GetVoltagePressure()             ip5306_get_bits(IP5306_REG_CHG_2, 0, 2)
#define IP5306_GetChargeCutoffVoltage()         ip5306_get_bits(IP5306_REG_CHG_2, 2, 2)
#define IP5306_GetChargeCCLoop()                ip5306_get_bits(IP5306_REG_CHG_3, 5, 1)
#define IP5306_GetVinCurrent()                  ip5306_get_bits(IP5306_REG_CHG_4, 0, 5)
#define IP5306_GetShortPressDetected()          ip5306_get_bits(IP5306_REG_READ_3, 0, 1)
#define IP5306_GetLongPressDetected()           ip5306_get_bits(IP5306_REG_READ_3, 1, 1)
#define IP5306_GetDoubleClickDetected()         ip5306_get_bits(IP5306_REG_READ_3, 2, 1)
#define IP5306_GetPowerSource()                 ip5306_get_bits(IP5306_REG_READ_0, 3, 1) // 0: BATTERY, 1: VIN (USB)
#define IP5306_GetBatteryFull()                 ip5306_get_bits(IP5306_REG_READ_1, 3, 1)
#define IP5306_GetOutputLoad()                  ip5306_get_bits(IP5306_REG_READ_2, 2, 1)
#define IP5306_GetLevelLeds()                   ((~ip5306_get_bits(IP5306_REG_READ_4, 4, 4)) & 0x0F)

#define IP5306_LEDS2PCT(byte)  \
  ((byte & 0x01 ? 25 : 0) + \
   (byte & 0x02 ? 25 : 0) + \
   (byte & 0x04 ? 25 : 0) + \
   (byte & 0x08 ? 25 : 0))

class MuseLuxe {
public:
    MuseLuxe();
    void begin();
    uint8_t getBatteryPercentage(); // Method to return battery percentage

    // Public methods to control the NeoPixel
    void setPixelColor(uint32_t color);
    void showPixel();
    uint32_t getColor(uint8_t r, uint8_t g, uint8_t b);

    // Public methods to get IP5306 statuses
    bool isCharging();             // Returns true if charging
    uint8_t getVinCurrent();       // Returns VIN current in mA
    uint8_t getVoltagePressure();  // Returns voltage pressure in mV

    // Audio methods
    void initAudio();
    void playSineWave(uint16_t frequency, uint16_t duration_ms);

    // Amplifier control
    void setAmplifier(bool enable); // New method to enable/disable the amplifier
    void setVolume(uint8_t volume);   // Set volume level (0-100%)
    uint8_t getVolume();              // Get current volume level

private:
    Adafruit_NeoPixel pixels;
    uint8_t getBatteryLevel();

    // IP5306 helper functions
    int ip5306_get_reg(uint8_t reg);
    int ip5306_set_reg(uint8_t reg, uint8_t value);
    uint8_t ip5306_get_bits(uint8_t reg, uint8_t index, uint8_t bits);
    void ip5306_set_bits(uint8_t reg, uint8_t index, uint8_t bits, uint8_t value);

    // ES8388 functions
    void ES8388_Write_Reg(uint8_t reg, uint8_t val);
    uint8_t ES8388_Read_Reg(uint8_t reg);
    void ES8388_Init();
    void I2S_Init();

    // Amplifier pin
    const gpio_num_t PA_PIN = GPIO_NUM_21; // Define as a constant member
    uint8_t currentVolume = 50;       // Initialize volume at 50%

};

#endif // MUSELUXE_H
