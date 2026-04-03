# LED Smart Mapper

A flexible IoT LED strip controller for ESP32 and Arduino, featuring MQTT-based remote control, virtual pixel mapping, and a library of built-in animation programs.

## Overview

LED Smart Mapper abstracts physical LEDs into independently controllable virtual "lights" (zones). Each zone can run its own animation program, respond to MQTT commands with JSON payloads, and be triggered by capacitive touch inputs — all configurable at compile time with no dynamic allocation overhead.

**Supported hardware:**
- ESP32 (WiFi)
- Arduino Mega (Ethernet)
- Teensy *(experimental)*

**Supported LED protocols:**
- APA102 (SPI, high refresh rate)
- WS2812 / NeoPixel

---

## Architecture

### Pixel Mapping

Physical LEDs are mapped to virtual `Light` objects via a pointer array (`CRGB** _leds`). This allows each light to own contiguous or non-contiguous pixels without copying data, enabling complex physical layouts from a flat LED buffer.

### Animation Programs

Each `Light` stores a function pointer to its active program (`int (Light::*_prog)(int x)`), making runtime program switching O(1) with no branching in the main loop. Programs maintain independent state through per-light counters, indices, and a parameter array.

Built-in programs:

| Program | Description |
|---|---|
| `solid` | Solid color fill |
| `chase` | Moving light with fade trail |
| `fade` | Full fade in/out cycle |
| `blink` | On/off strobe |
| `warm_glow` | Random twinkling warmth effect |
| `christmas` | Cycling multicolor pattern |
| `lfo` | Sine wave brightness oscillation |

### MQTT Control Protocol

Lights subscribe to individual MQTT topics and accept JSON command payloads:

```json
{
  "on": true,
  "hue": 210,
  "sat": 80,
  "bri": 75,
  "program": "chase",
  "params": [10, 3, 0, 0],
  "speed": 5
}
```

Commands can target a specific light, all lights, or broadcast to the whole device. Reconnection logic automatically re-registers all subscriptions on broker reconnect.

### Touch Input

Capacitive touch is handled by `TouchControl`, which uses hysteresis-based detection (not simple thresholds) to fire distinct callbacks for press, hold, and release events — debounced over configurable read-count windows.

---

## Configuration

Everything is configured via a header file (`config.h`, based on `config.example.h`). There is no runtime configuration overhead.

### Board

```cpp
#define IS_ESP32     // ESP32 with WiFi
#define IS_MEGA      // Arduino Mega with Ethernet
#define IS_TEENSY    // Teensy (experimental)
```

### LED Type

```cpp
#define IS_APA102    // SPI-based (CLK + DATA)
#define IS_WS2812    // Single-wire NeoPixel
```

### Parameters

```cpp
#define NUM_LEDS        60    // Total physical LEDs
#define NUM_LIGHTS      4     // Virtual light zones
#define NUM_CONTROLS    2     // Capacitive touch inputs
#define NUM_PARAMS      4     // Per-light animation parameters
#define BRIGHTNESS_SCALE 200  // Global brightness cap (0–255)
```

### Optional Features

```cpp
#define TOUCH       // Enable capacitive touch controls
#define BUMP_LED    // Sacrifice first LED to boost 3.3v→5v for APA102
#define DEBUG       // Enable serial debug output
```

---

## Dependencies

Install via Arduino Library Manager:

- [FastLED](https://github.com/FastLED/FastLED) — LED control and color math
- [PubSubClient](https://github.com/knolleary/pubsubclient) — MQTT client
- [ArduinoJson](https://arduinojson.org/) — JSON payload parsing

---

## Setup

1. Copy `config.example.h` to `config.h` and fill in your WiFi credentials, MQTT broker address, and hardware configuration.
2. Define your pixel map in `mqtt_ledstrip.ino` — assign physical LED indices to each virtual light.
3. Flash to your board via Arduino IDE or `make`.

---

## Experimental

- **ArtNet** — DMX-over-network support is partially implemented but known to be unstable. Useful as a starting point for lighting console integration.

## Planned

- Bluetooth control
- Dynamic (runtime) configuration
