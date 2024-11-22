// ESP Manga NeoPixel (c) 2024 RASPiAudio
// This code displays the French flag colors on the Manga's programmable RGB LED


#include <Adafruit_NeoPixel.h>

// Define the pin connected to the NeoPixel LED
#define PIN        22 // Pin for the RGB LED on Muse 

// Define the number of NeoPixels
#define NUMPIXELS 1 // Muse Luxe has 1 RGB LED

// Set up the NeoPixel library with number of pixels and pin configuration
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define DELAYVAL 500 // Time (in milliseconds) to pause between colors

void setup() {
  // Initialize the NeoPixel strip object (REQUIRED for the Muse Luxe's RGB LED)
  pixels.begin();
}

void loop() {
  // Display blue 
  pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
  pixels.show();
  delay(DELAYVAL);

  // Display white 
  pixels.setPixelColor(0, pixels.Color(255, 255, 255)); // White
  pixels.show();
  delay(DELAYVAL);

  // Display red 
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
  pixels.show();
  delay(DELAYVAL);

  // Turn off the LED for 1 second
  pixels.setPixelColor(0, pixels.Color(0, 0, 0)); // Black (off)
  pixels.show();
  delay(1000);
}
