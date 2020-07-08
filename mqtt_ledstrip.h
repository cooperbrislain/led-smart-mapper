#ifndef MQTT_LEDSTRIP_MQTT_LEDSTRIP_H
#define MQTT_LEDSTRIP_MQTT_LEDSTRIP_H

#include "config.h"

#include <SPI.h>
#include <FastLED.h>

#include "light.h"
#include "touchcontrol.h"

#ifdef IS_MEGA
    #define DATA_PIN 42
    #define CLOCK_PIN 43
    #define USE_ETHERNET
#endif
#ifdef IS_ESP32
    #define DATA_PIN 21
    #define CLOCK_PIN 22
    #define USE_WIFI
#endif

#ifndef NO_NETWORK
    #include <Dns.h>
    #include <Dhcp.h>
    #include <PubSubClient.h>
    #include <ArduinoJson.h>
#endif

#ifdef ARTNET
    #include <Artnet.h>
#endif
#ifdef USE_WIFI
    #include <WiFi.h>
#endif
#ifdef USE_ETHERNET
    #include <Ethernet.h>
#endif

#ifndef DEVICE_NAME
    #define DEVICE_NAME "led-bridge"
#endif

#ifdef TOUCH
#define TOUCH_PIN T0
    #ifndef TOUCH_THRESHOLD
        #define TOUCH_THRESHOLD 50
    #endif
#endif

#ifndef NUM_LEDS
    #define NUM_LEDS 25
#endif
#ifndef NUM_LIGHTS
    #define NUM_LIGHTS 1
#endif
#ifndef NUM_PARAMS
    #define NUM_PARAMS 3
#endif
#ifndef BRIGHTNESS_SCALE
    #define BRIGHTNESS_SCALE 50
#endif

#define halt(s) { Serial.println(F( s )); while(1);  }

void reconnect();
void blink();
void blink_rainbow();
void blackout();

void mqtt_callback(char* topic, byte* payload, unsigned int length);

#endif //MQTT_LEDSTRIP_MQTT_LEDSTRIP_H
