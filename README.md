# Muse Library

This is a library to ease devlopment with Raspiaudio's muse devices, for now it supports [ESP32 Muse devices](https://raspiaudio.com/muse/) :
- Muse Luxe
- Muse Radio 
- Muse Proto 
- Muse Manga
  

## Overview

The **Muse Library** is an Arduino library designed for the ESP32-based Muse devices. It provides seamless control over the Muse  NeoPixel LED, interfaces with the ES8388 codec for audio, and includes support for various other peripherals. 

## Features


- **OpenAi chatgpt integration** for realtime API and classic STT-LML-TTS
- **Audio Playback**: Streaming URL or local audio file on SD card
- **Audio recording**
- **Battery monitoring**: Control audio playback using the ES8388 codec and integrate streaming or audio functionalities.
- **NeoPixel Control**: Easily set and display colors on the Muse Luxe's integrated NeoPixel LED.

## Installation

### Using Arduino Library Manager

1. Open the Arduino IDE.
2. Navigate to **Sketch** > **Include Library** > **Manage Libraries**.
3. In the Library Manager, search for `Muse_library`.
4. Click **Install** on the Muse library by `Raspiaudio`.


## Muse radio arduino configuration
<img width="402" height="587" alt="image" src="https://github.com/user-attachments/assets/3cccf144-1632-437a-ba7c-296fc5967c82" />


## OpenAI examples
You will need an API key form OpenAi, add your key in OPENAI_API_KEY[] = "sk-YOUR ENTER API KEY HERE";


## Dependencies
- Tested on ESP32 3.3.0
- ESP32-audioI2S-master@3.4.0
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) by Adafruit
- [ESP32Encoder](https://github.com/madhephaestus/ESP32Encoder)
- [ESP32-A2DP](https://github.com/pschatzmann/ESP32-A2DP)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
- [esp32-sh1106-oled](https://github.com/davidperrenoud/Adafruit_SH1106) **replace the User_Setup.h of this repository in the TFT-eSPI library directory to apply the screen preferences**
- ArduinoJson@7.4.2
- Arduino sockets
- ArduinoHttpClient



Ensure that the libraries are installed before using the Muse_library.



