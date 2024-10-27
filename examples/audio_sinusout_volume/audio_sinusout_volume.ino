#include <Arduino.h>
#include "museluxe.h"

// Define the MuseLuxe object
MuseLuxe muse;

// Define button pins (adjust these to your actual pin numbers)
#define BUTTON_PLUS_PIN 32
#define BUTTON_MINUS_PIN 33

void setup() {
  Serial.begin(115200);
  muse.begin();

  // Enable the amplifier
  muse.setAmplifier(true);
  delay(500);

  // Set initial volume to 50%
  muse.setVolume(50);

  // Initialize button pins
  pinMode(BUTTON_PLUS_PIN, INPUT_PULLUP);
  pinMode(BUTTON_MINUS_PIN, INPUT_PULLUP);

  // Set NeoPixel to indicate ready status
  muse.setPixelColor(muse.getColor(0, 255, 0)); // Green
  muse.showPixel();
}

void loop() {
  // Volume control with buttons
  static uint32_t lastButtonTime = 0;
  uint32_t currentTime = millis();

  if (currentTime - lastButtonTime > 200) { // Debounce delay
    if (digitalRead(BUTTON_VOL_PLUS) == LOW) {
      uint8_t volume = muse.getVolume();
      if (volume < 100) {
        volume += 5; // Increase volume by 5%
        muse.setVolume(volume);
        Serial.printf("Volume increased to %d%%\n", volume);
      } else {
        Serial.println("Volume is already at maximum.");
      }
      lastButtonTime = currentTime;
    }

    if (digitalRead(BUTTON_VOL_MINUS) == LOW) {
      uint8_t volume = muse.getVolume();
      if (volume > 0) {
        volume -= 5; // Decrease volume by 5%
        muse.setVolume(volume);
        Serial.printf("Volume decreased to %d%%\n", volume);
      } else {
        Serial.println("Volume is already at minimum.");
      }
      lastButtonTime = currentTime;
    }
  }

  // Play a sine wave continuously
  muse.playSineWave(200, 100); // Play 200Hz sine wave for 100ms
}
