#include <Arduino.h>
#include "museluxe.h"

// Define the MuseLuxe object
MuseLuxe muse;

// Path to the WAV file in SPIFFS
const char* wavFile = "/audio.wav";

void setup() {
  Serial.begin(115200);
  while (!Serial) { ; } // Wait for Serial to initialize

  muse.begin();

  // Enable the amplifier
  muse.setAmplifier(true); // Enable amplifier
  delay(500); // Wait for the amplifier to stabilize

  Serial.println("Starting WAV playback...");

  // Play the WAV file from SPIFFS
  muse.playWavFile(wavFile);

  Serial.println("WAV playback completed.");

  // Optionally, disable the amplifier after playback
  muse.setAmplifier(false);
}

void loop() {
  // Do nothing or implement other functionalities
}
