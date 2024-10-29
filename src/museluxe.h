// museluxe.h - Header file for ESP Muse Luxe NeoPixel, Battery Management System (BMS) using IP5306, and ES8388 Audio Codec
// (c) 2024 RASPiAudio

#ifndef MUSELUXE_H
#define MUSELUXE_H

#include <Arduino.h>
#include <Wire.h>
#include "ES8388.h"  // https://github.com/schreibfaul1/es8388
#include "Audio.h"   // check this for more examples https://github.com/schreibfaul1/ESP32-audioI2S
#include <Adafruit_NeoPixel.h> // https://github.com/adafruit/Adafruit_NeoPixel


//SD card
#define SD_CS         13

// GPIOs for SPI
#define SPI_MOSI      15
#define SPI_MISO      2
#define SPI_SCK       14

// I2S GPIOs
#define I2S_SDOUT     26
#define I2S_BCLK       5
#define I2S_LRCK      25
#define I2S_MCLK       0

// I2C GPIOs
#define IIC_CLK       23
#define IIC_DATA      18

// Amplifier enable
#define GPIO_PA_EN    21

// Pin Definitions
#define NEOPIXEL_PIN 22        // Pin for the RGB LED on Muse Luxe
#define NUMPIXELS 1            // Muse Luxe has 1 RGB LED

// Buttons definition
#define BUTTON_PAUSE GPIO_NUM_12  // Pause/Play button
#define BUTTON_VOL_MINUS GPIO_NUM_32 // Volume - button
#define BUTTON_VOL_PLUS GPIO_NUM_19  // Volume + button

// Battery Level Thresholds
#define BATTERY_FULL 75
#define BATTERY_LOW  25
#define BATTERY_PIN 34 //for legacy ADC pin (not the I2C bms mode)

// IP5306 for Battery management system I2C Address and Registers
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


private:
    Adafruit_NeoPixel pixels;
    uint8_t getBatteryLevel();

    // IP5306 helper functions
    int ip5306_get_reg(uint8_t reg);
    int ip5306_set_reg(uint8_t reg, uint8_t value);
    uint8_t ip5306_get_bits(uint8_t reg, uint8_t index, uint8_t bits);
    void ip5306_set_bits(uint8_t reg, uint8_t index, uint8_t bits, uint8_t value);


};

#endif // MUSELUXE_H
