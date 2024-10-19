# MuseLuxe Library

![MuseLuxe Logo](https://github.com/YourUsername/MuseLuxe/blob/main/assets/logo.png?raw=true)

## Overview

The **MuseLuxe Library** is an Arduino library designed for the ESP32-based Muse Luxe device. It provides seamless control over the Muse Luxe's NeoPixel LED and interfaces with the IP5306 Battery Management System (BMS). This library abstracts the complexities of NeoPixel management and battery monitoring, allowing developers to focus on building innovative applications with ease.

## Features

- **NeoPixel Control**: Easily set and display colors on the Muse Luxe's integrated NeoPixel LED.
- **Battery Monitoring**: Retrieve accurate battery percentage, charging status, VIN current, and voltage pressure using the IP5306 module.
- **Encapsulation**: Maintains proper encapsulation by providing public methods for interaction, hiding the underlying implementation details.
- **Extensibility**: Designed to be easily extendable for additional functionalities and integrations.
- **Debugging Support**: Logs essential battery and charging parameters to the Serial Monitor for debugging and monitoring purposes.

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
4. Click **Install** on the MuseLuxe library by `YourUsername`.

### Manual Installation

1. Download the [latest release](https://github.com/YourUsername/MuseLuxe/releases) of the MuseLuxe library.
2. Extract the downloaded ZIP file.
3. Move the extracted folder to your Arduino libraries directory:
   - **Windows**: `Documents\Arduino\libraries\`
   - **macOS/Linux**: `~/Arduino/libraries/`
4. Restart the Arduino IDE to recognize the new library.

## Dependencies

- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) by Adafruit
- [Arduino Wire](https://www.arduino.cc/en/Reference/Wire) Library (built-in with Arduino IDE)

Ensure that the **Adafruit NeoPixel** library is installed before using the MuseLuxe library.

## Usage

### Including the Library

```cpp
#include <Arduino.h>
#include "museluxe.h"
