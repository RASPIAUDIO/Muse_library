#ifndef MUSE_LUXE_DEFINES_H
#define MUSE_LUXE_DEFINES_H

// Define the pin connected to the NeoPixel LED
#define NEOPIXEL_PIN        22 // Pin for the RGB LED on Muse Luxe

// Define the number of NeoPixels
#define NUMPIXELS 1 // Muse Luxe has 1 RGB LED

// Define battery level pin
#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_6 // ADC channel for battery monitoring (GPIO 33)

// Battery level thresholds
#define BATTERY_FULL 2300
#define BATTERY_LOW  1800

// Buttons definition
#define BUTTON_PAUSE GPIO_NUM_12  // Pause/Play button
#define BUTTON_VOL_MINUS GPIO_NUM_32 // Volume - button
#define BUTTON_VOL_PLUS GPIO_NUM_19  // Volume + button

#endif // MUSE_LUXE_DEFINES_H
