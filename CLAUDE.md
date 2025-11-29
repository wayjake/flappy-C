# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Flappy Bird game for Arduino/ESP32 with an SH1106 OLED display (128x64). The game runs on the Wokwi emulator or physical hardware.

## Hardware Configuration

- **Display**: SH1106G OLED (128x64) via I2C (SDA=41, SCL=42, address 0x3C)
- **Button**: Pin 20 (INPUT_PULLUP, active LOW)
- **Buzzer**: Pin 5

## Build & Upload

Use Arduino IDE or PlatformIO:
```bash
# Arduino CLI example
arduino-cli compile --fqbn esp32:esp32:esp32s3 FlappyBird
arduino-cli upload --fqbn esp32:esp32:esp32s3 -p /dev/ttyUSB0 FlappyBird
```

For Wokwi simulation, open the .ino file in VS Code with the Wokwi extension.

## Architecture

**FlappyBird.ino** - Single-file game with all logic:
- Game states: `STATE_TITLE` (start screen) and `STATE_PLAYING` (active gameplay)
- Physics: Gravity-based bird movement with flap mechanic (FLAP_DURATION controls lift time)
- Pipes: 4 pipes cycle across screen, each with random gap position (GAP_MIN_Y to GAP_MAX_Y)
- Collision: Checks bird bounds against screen edges and pipe rectangles

**images.h** - XBM bitmap assets (background, birdSprite) stored in PROGMEM

**fontovi.h** - Custom Rancho Regular 14pt font data (not currently used in main code)

## Key Implementation Details

- XBM bitmaps use LSB-first bit ordering; `drawXbm()` converts to MSB-first for Adafruit_GFX
- All sprite/image data uses PROGMEM to save RAM
- Score increments when pipe moves off left edge (pipeX < -PIPE_WIDTH - 1)
- Button debouncing via `buttonWasPressed` flag prevents multiple flaps per press

## Git Commits

Do not add Claude watermarks or Co-Authored-By lines to commit messages.
