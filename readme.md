# MuseLuxe Library

![MuseLuxe Logo](https://github.com/YourUsername/MuseLuxe/blob/main/assets/logo.png?raw=true)

## Overview

The **MuseLuxe Library** is an Arduino library designed for the ESP32-based Muse Luxe device. It provides seamless control over the Muse Luxe's NeoPixel LED, interfaces with the ES8388 codec for audio, and includes support for various other peripherals. This library abstracts the complexities of NeoPixel management and audio control, allowing developers to focus on building innovative applications with ease.

## Features

- **NeoPixel Control**: Easily set and display colors on the Muse Luxe's integrated NeoPixel LED.
- **Audio Playback**: Control audio playback using the ES8388 codec and integrate streaming or audio functionalities.
- **Encapsulation**: Maintains proper encapsulation by providing public methods for interaction, hiding the underlying implementation details.
- **Extensibility**: Designed to be easily extendable for additional functionalities and integrations.
- **Debugging Support**: Logs essential parameters to the Serial Monitor for debugging and monitoring purposes.

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

