# MQTT-LEDStrip

A flexible codebase for IOT programmable LED strips.

### Features
* Wifi/Ethernet
* MQTT
* Pixel mapping
* Programs
* Transitions

### Experimental/Buggy
* ArtNet

### Future Features
* Bluetooth

### Configuration
Right now almost everything is configured through a header file. 
In the future, this will be something more dynamic. Here are the 
defines you will need to configure to get things working for 
your project.

#### Board Spec
* `IS_ESP32` - for ESP32 boards
* `IS_MEGA` - For Arduino Mega 8266
* `IS_TEENSY` - For Teensy (Experimental, probably broken)

#### LED Spec
* `IS_APA102` | `IS_WS2812`

#### Parameters
* `NUM_LEDS` - the total number of LEDs
* `NUM_LIGHTS` - the number of virtual lights
* `NUM_CONTROLS` - the number of touch controls
* `NUM_PARAMS` - the number of params on each light
* `BRIGHTNESS_SCALE` - the global LED brightness scale

#### Features
* `TOUCH` - For capacitive touch controls
* `BUMP_LED` - Use first LED to bump voltage from 3.3v to 5v (APA102)

### Dependencies
* FastLED
* PubSubClient
* ArduinoJson

