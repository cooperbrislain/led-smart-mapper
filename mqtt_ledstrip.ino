#define MQTT_DEBUG
#include "SPI.h"
#include <Ethernet.h>
#include <Dns.h>
#include <Dhcp.h>
#include <PubSubClient.h>
#include "FastLED.h"

#define DATA_PIN 7 // signal for LED strip
#define NUM_LEDS 10 // total number of LEDs for all strips

#define NUM_LIGHTS 3

#define halt(s) { Serial.println(F( s )); while(1);  }

void mqtt_callback(char* topic, byte* payload, unsigned int length);
void reconnect();

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
        void set_rgb(CRGB color);
        void set_hsv(int status, int sat, int val);
        void subscribe();
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
byte mac[] = { 0xDA, 0x3D, 0xB3, 0xF3, 0xF0, 0x3D };

const char* mqtt_server = "mqtt.spaceboycoop.com";
const int mqtt_port = 1883;
const char* mqtt_username = "spaceboycoop";
const char* mqtt_key = "r7rObnC6i2paWPxeEMuWiF";

CRGB leds[NUM_LEDS];
Light lights[NUM_LIGHTS];

EthernetClient eth_client;
PubSubClient mqtt_client(eth_client);

void setup() {
    Serial.begin(115200);
    Serial.println("Starting up MQTT LED Controller");
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
        delay(25);
    }
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black; // go black again
    }
    FastLED.show();
    delay(1000);
    mqtt_client.setServer(mqtt_server, mqtt_port);
    mqtt_client.setCallback(mqtt_callback);

    // initialize lights;
    // this really needs to read from a config file
    /*
    lights[0] = Light("case", &leds[0], 2);
    lights[1] = Light("rear-lower", &leds[2], 1);
    lights[2] = Light("drawer-1", &leds[3], 1);
    lights[3] = Light("drawer-2", &leds[4], 1);
    lights[4] = Light("drawer-3", &leds[5], 1);
    lights[5] = Light("drawer-4", &leds[6], 1);
    lights[6] = Light("roof-passenger", &leds[7], 17);
    lights[7] = Light("shelf", &leds[23], 4);
    lights[8] = Light("roof-driver", &leds[27], 15);
    lights[9] = Light("rear-upper", &leds[42], 3);
    */

    lights[0] = Light("test-0", &leds[0], 10);
    lights[1] = Light("test-1", &leds[0], 5);
    lights[2] = Light("test-2", &leds[5], 5);

    // turn the lights on one at a time
    for (int i=0; i<NUM_LIGHTS; i++) {
        lights[i].turn_on();
        delay(25);
        lights[i].turn_off();
        delay(250);
    }

    for (int i=0; i<NUM_LIGHTS; i++) {
        lights[i].turn_off();
    }

    Ethernet.begin(mac);

    Serial.println(F("Connecting..."));
    if(!Ethernet.begin(mac)) {
        Serial.println(F("Ethernet configuration failed."));
        for(;;);
    }
    Serial.println(F("Ethernet configured via DHCP"));
    Serial.print("IP address: ");
    Serial.println(Ethernet.localIP());
    Serial.println();

    delay(1500);
}


void loop() {
    if (!mqtt_client.connected()) {
        Serial.println("Reconnecting...");
        reconnect();
    }
    mqtt_client.loop();
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    payload[length] = '\0';
    char* tmp = strtok(topic,"/.");
    tmp = strtok(NULL,"/.");
    char* name = strtok(NULL,"/.");
    char* prop = strtok(NULL,"/.");
    Serial.println(name);
    Serial.println(prop);
    Serial.println((char *)payload);
    for (int i=0; i<NUM_LIGHTS; i++) {
        if(strcmp(name, lights[i].get_name()) == 0) {
            if(strcmp(prop, "color") == 0) {
                CRGB payload_color;
                sscanf((char *)payload, "#%2x%2x%2x", &payload_color.r, &payload_color.g, &payload_color.b);
                lights[i].set_rgb(payload_color);
            }
        }
    }
    if(strcmp(topic,"deliverator/lights/color") == 0) {
        Serial.println("Message received from deliverator/lights/color");
        CRGB payload_color;
        sscanf((char *)payload, "#%2x%2x%2x", &payload_color.r, &payload_color.g, &payload_color.b);
        Serial.println("Changing light[1]");
        lights[1].set_rgb(payload_color);
    }
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

void Light::subscribe() {
    if (mqtt_client.connected()) {
        char* feed;
        sprintf(feed, "deliverator/lights/%s/color", _name.c_str());
        mqtt_client.subscribe(feed);
        sprintf(feed, "deliverator/lights/%s/status", _name.c_str());
        mqtt_client.subscribe(feed);
        sprintf(feed, "deliverator/lights/%s/brightness", _name.c_str());
        mqtt_client.subscribe(feed);
        sprintf(feed, "deliverator/lights/%s/hue", _name.c_str());
        mqtt_client.subscribe(feed);
    }
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

void Light::set_rgb(CRGB color) {
    _color = color;
    update();
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

void reconnect() {
  // Loop until we're reconnected
    while (!mqtt_client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (mqtt_client.connect("arduinoClient",mqtt_username,mqtt_key)) {
            Serial.println("connected");
            // Once connected, publish an announcement...
            if(!mqtt_client.publish("deliverator/lights/color","OFF")) {
               Serial.println("Failed to publish");
            }
            // ... and resubscribe
            if(!mqtt_client.subscribe("deliverator/lights/color")) {
                Serial.println("Subscribe to deliverator/lights/color failed.");
            }
            char *name;
            for(int i=0; i<NUM_LIGHTS; i++) {
                Serial.print("Subscribine to feeds for light:");
                lights[i].subscribe();
            }
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqtt_client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}