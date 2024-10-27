#include <Arduino.h>
#include "museluxe.h" // Ensure the library is correctly named and included

// Define the MuseLuxe object
MuseLuxe muse;

void setup() {
  Serial.begin(115200);
  muse.begin();

  // Enable the amplifier
  muse.setAmplifier(true); // Enable amplifier
  delay(500); // Wait for the amplifier to stabilize

}

void loop() {
  // Set NeoPixel to blue to indicate start
  muse.setPixelColor(muse.getColor(0, 0, 255));
  muse.showPixel();

  // Play a 200Hz sine wave for 5 seconds
  Serial.println("Playing 200Hz sine wave");
  muse.playSineWave(200, 5000);

  // Turn off NeoPixel
  muse.setPixelColor(muse.getColor(0, 0, 0));
  muse.showPixel();

  // Disable the amplifier after playback (optional)
  muse.setAmplifier(false);
  delay(2000); // Wait for a second
  Serial.println("pause");
}
