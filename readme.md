# Muse Library
This is a library to ease devlopment with Raspiaudio's muse device, for now it supports :
- Muse Luxe [buy here]([https://github.com/YourUsername/MuseLuxe/releases](https://raspiaudio.com/product/esp-muse-luxe)
- Muse Radio (soon)
- Muse Proto (soon)
- Muse MnCast (soon)
  

## Overview

The **MuseLuxe Library** is an Arduino library designed for the ESP32-based Muse devices. It provides seamless control over the Muse  NeoPixel LED, interfaces with the ES8388 codec for audio, and includes support for various other peripherals. 

## Features

- **NeoPixel Control**: Easily set and display colors on the Muse Luxe's integrated NeoPixel LED.
- **Audio Playback**: Control audio playback using the ES8388 codec and integrate streaming or audio functionalities.
- **Batery monitoring**: Control audio playback using the ES8388 codec and integrate streaming or audio functionalities.

- Audio recording coming soon

## Table of Contents

- [Installation](#installation)
- [Dependencies](#dependencies)
- [Usage](#usage)
  - [Including the Library](#including-the-library)
  - [Basic Example](#basic-example)
- [Library Reference](#library-reference)
  - [Public Methods](#public-methods)
- [Example Sketch](#example-sketch)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

## Installation

### Using Arduino Library Manager

1. Open the Arduino IDE.
2. Navigate to **Sketch** > **Include Library** > **Manage Libraries**.
3. In the Library Manager, search for `MuseLuxe`.
4. Click **Install** on the MuseLuxe library by `Raspiaudio`.

### Manual Installation

1. Download the [latest release](https://github.com/YourUsername/MuseLuxe/releases) of the MuseLuxe library.
2. Extract the downloaded ZIP file.
3. Move the extracted folder to your Arduino libraries directory:
   - **Windows**: `Documents\Arduino\libraries\`
   - **macOS/Linux**: `~/Arduino/libraries/`
4. Restart the Arduino IDE to recognize the new library.

## Dependencies
- Tested on ESP32 version 2.0.13 
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) by Adafruit
- [ES8388](https://github.com/schreibfaul1/es8388) by schreibfaul1
- [Audio](https://github.com/schreibfaul1/ESP32-audioI2S) by schreibfaul1


Ensure that the **Adafruit NeoPixel**, **ES8388**, and **Audio** libraries are installed before using the MuseLuxe library.

## Usage

### Including the Library

```cpp
#include "museluxe.h"

