#define MQTT_DEBUG
#include "SPI.h"
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Dns.h>
#include <Dhcp.h>
#include "FastLED.h"
#include <SRAM.h>

#define DATA_PIN 7 // signal for LED strip
#define NUM_LEDS 51 // total number of LEDs for all strips

#define NUM_LIGHTS 10

#define halt(s) { Serial.println(F( s )); while(1);  }

void mqtt_callback();

class Light {
    public:
        Light(String name, CRGB* leds, int num_leds);
        Light();

        void turn_on();
        void turn_off();
        void toggle();
        void set_brightness(int val);
        void set_status(int val);
        void set_saturation(int val);
        void set_hsv(int status, int sat, int val);
        const char* get_name();

    private:
        CRGB* _leds;
        CRGB _color;
        int _num_leds;
        int _last_brightness;
        int _status;
        String _name;
        String _feed_hue;
        String _feed_brightness;
        String _feed_status;
        void update();
};

// Ethernet Vars
byte mac[]    = {  0xD3, 0x3D, 0xB4, 0xF3, 0xF3, 0x3D };

SRAM sram(4, SRAM_1024);

const char* mqtt_server = "broker.mqtt-dashboard.com";
const int mqtt_port = 1883;
const char* mqtt_username = "cbrislain";
const char* mqtt_key = "a95102c1e84b4edb82aea735a5e7f82c";

CRGB leds[NUM_LEDS];
Light lights[NUM_LIGHTS];

EthernetClient client_eth;
PubSubClient client_mqtt(mqtt_server, mqtt_port, mqtt_callback, client_eth, sram);

void setup() {
    Serial.begin(115200);
    Serial.println(F("Starting up MQTT LED Controller"));
    delay(10);

    // initialize fastled and test all lights
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black; // go black first
    }
    FastLED.show();
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::White; // fill with white 1 by 1
        FastLED.show();
        delay(10);
    }
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black; // go black again
    }
    FastLED.show();

    // initialize lights;
    // this really needs to read from a config file
    lights[0] = Light("case", &leds[0], 4);
    lights[1] = Light("rear_lower", &leds[4], 1);
    lights[2] = Light("drawer_1", &leds[5], 1);
    lights[3] = Light("drawer_1", &leds[6], 1);
    lights[4] = Light("drawer_3", &leds[7], 1);
    lights[5] = Light("drawer_4", &leds[8], 1);
    lights[6] = Light("roof_passenger", &leds[9], 17);
    lights[7] = Light("shelf", &leds[26], 4);
    lights[8] = Light("roof_driver", &leds[30], 16);
    lights[9] = Light("rear_upper", &leds[46], 4);

    // turn the lights on one at a time
    for (int i=0; i<10; i++){
        for (int i=0; i<NUM_LIGHTS; i++) {
            lights[i].turn_on();
            delay(25);
            lights[i].turn_off();
            delay(75);
        }
    }

    delay(10);
    if (!Ethernet.begin(mac)) {
        Serial.println(F("Unable to configure Ethernet using DHCP"));
        for (;;);
    }
    Serial.println(F("Ethernet configured via DHCP"));
    Serial.print("IP address: ");
    Serial.println(Ethernet.localIP());
    Serial.println();

    if (client_mqtt.connect("ArdCli")) {
        client_mqtt.publish("outTopic", "Hello, world");
        client_mqtt.subscribe("inTopic");
    }

    sram.begin();
    sram.seek(1);

    // rest of config goes here;

    for (int i=0; i<NUM_LIGHTS; i++) {
        lights[i].turn_off();
    }
}


void loop() {
    client_mqtt.loop();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    sram.seek(1);

  // do something with the message
  for(uint8_t i=0; i<length; i++) {
    Serial.write(sram.read());
  }
  Serial.println();

  // Reset position for the next message to be stored
  sram.seek(1);
}

Light::Light() {
    _color = CRGB::Black;
    _status = 0;
    _num_leds = 0;
    _leds = 0;
    _feed_status = "";
    _feed_brightness = "";
    _feed_status = "";
    _name = "light";
}

Light::Light(String name, CRGB* leds, int num_leds) {
    _color = CRGB::Black;
    _status = 0;
    _num_leds = num_leds;
    _leds = leds;
    _name = name;
}

void Light::update() {
    for (int i=0; i<_num_leds; i++) {
        _leds[i] = _color;
    }
    FastLED.show();
}

void Light::turn_on() {
    _color = CRGB::White;
    update();
}

void Light::turn_off() {
    _color = CRGB::Black;
    update();
}

void Light::toggle() {
    if (_status == 1) {
        turn_off();
    } else {
        turn_on();
    }
}

void Light::set_brightness(int val) {
    _color = CHSV(255,0,val);
    update();
}

void Light::set_status(int val) {
    _color = CHSV(val, 255, 255);
    update();
}

void Light::set_saturation(int val) {
    _color = CHSV(255, val, 255);
    update();
}

void Light::set_hsv(int status, int sat, int val) {
    _color = CHSV(status, sat, val);
    update();
}

const char* Light::get_name() {
    return _name.c_str();
}