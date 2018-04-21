#define MQTT_DEBUG
#include "SPI.h"
#include <Ethernet.h>
#include <Dns.h>
#include <Dhcp.h>
#include <PubSubClient.h>
#include "FastLED.h"

#define DATA_PIN 7 // signal for LED strip
#define NUM_LEDS 51 // total number of LEDs for all strips

#define NUM_LIGHTS 10

#define halt(s) { Serial.println(F( s )); while(1);  }

void mqtt_callback(char* topic, byte* payload, unsigned int length);
void reconnect();

class Light {
    public:
        Light(String name, CRGB* leds, int num_leds);
        Light();

        const char* get_name();
        void turn_on();
        void turn_off();
        void toggle();
        void set_hue(int val);
        void set_brightness(int val);
        void set_saturation(int val);
        void set_rgb(CRGB);
        void set_hsv(int hue, int sat, int val);
        void set_hsv(CHSV);
        void set_program(int prog_id);
        CRGB get_rgb();
        CHSV get_hsv();
        void initialize();
        void update();


    private:
        CRGB* _leds;
        CRGB _color;
        int _num_leds;
        int _last_brightness;
        bool _onoff;
        String _name;
        int _count;
        void *_prog();
        void add_to_homebridge();
        void subscribe(String);

        void _prog_solid();
        void _prog_fade();
        void _prog_chase();
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
    lights[0] = Light("case", &leds[0], 4);
    lights[1] = Light("rear-lower", &leds[4], 1);
    lights[2] = Light("drawer-1", &leds[5], 1);
    lights[3] = Light("drawer-2", &leds[6], 1);
    lights[4] = Light("drawer-3", &leds[7], 1);
    lights[5] = Light("drawer-4", &leds[8], 1);
    lights[6] = Light("roof-passenger", &leds[9], 17);
    lights[7] = Light("shelf", &leds[26], 4);
    lights[8] = Light("roof-driver", &leds[30], 16);
    lights[9] = Light("rear-upper", &leds[46], 3);
    */
    //for testing
    lights[0] = Light("case", &leds[0], 2);
    lights[1] = Light("rear-lower", &leds[1], 1);
    lights[2] = Light("drawer-1", &leds[2], 1);
    lights[3] = Light("drawer-2", &leds[3], 1);
    lights[4] = Light("drawer-3", &leds[4], 1);
    lights[5] = Light("drawer-4", &leds[5], 1);
    lights[6] = Light("roof-passenger", &leds[0], 5);
    lights[7] = Light("shelf", &leds[5], 5);
    lights[8] = Light("roof-driver", &leds[3], 5);
    lights[9] = Light("rear-upper", &leds[7], 3);

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
    for(int i=0; i<NUM_LIGHTS; i++) {
        lights[i].update();
    }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    payload[length] = '\0';
    char* tmp = strtok(topic,"/");
    tmp = strtok(NULL,"/");
    char* name = strtok(NULL,"/");
    char* prop = strtok(NULL,"/");
    Serial.println(name);
    Serial.println(prop);
    Serial.println((char *)payload);
    for (int i=0; i<NUM_LIGHTS; i++) {
        if (strcmp(name, lights[i].get_name()) == 0) {
            if (strcmp(prop, "On") == 0) {
                if(strcmp((char *)payload, "true") == 0) {
                    Serial.println("Turning On");
                    lights[i].turn_on();
                } else {
                    Serial.println("Turning Off");
                    lights[i].turn_off();
                }
            }
            if (strcmp(prop, "Hue") == 0) {
                int val = atoi((char *)payload);
                val = val * 255 / 360;
                Serial.print("Setting hue to ");
                Serial.println(val);
                lights[i].set_hue(val);
            }
            if (strcmp(prop, "Brightness") == 0) {
                int val = atoi((char *)payload);
                val = val * 255 / 100;
                Serial.print("Setting brightness to ");
                Serial.println(val);
                lights[i].set_brightness(val);
            }
            if (strcmp(prop, "Saturation") == 0) {
                int val = atoi((char *)payload);
                val = val * 255 / 100;
                Serial.print("Setting saturation to ");
                Serial.println(val);
                lights[i].set_saturation(val);
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
                lights[i].initialize();
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

// Light member functions

Light::Light() {
    _color = CRGB::Black;
    _onoff = 0;
    _num_leds = 0;
    _leds = 0;
    _name = "light";
    _prog() = _prog_solid;
    _count = 0;
}

Light::Light(String name, CRGB* leds, int num_leds) {
    _color = CRGB::Black;
    _onoff = 0;
    _num_leds = num_leds;
    _leds = leds;
    _name = name;
    _prog() = _prog_solid;
    _count = 0;
}

void Light::subscribe(String prop) {
    char tmp[128];
    sprintf(tmp, "deliverator/lights/%s/%s", _name.c_str(), prop.c_str());
    String feed = tmp;
    if(!mqtt_client.subscribe(feed.c_str())) {
        Serial.print("Failed to subscribe to feed: ");
        Serial.println(feed);
    } else {
        Serial. print("Subscribed to feed: ");
        Serial.println(feed);
    }
}

void Light::initialize() {
    if (mqtt_client.connected()) {
        Serial.print("Subscribing to feeds for light:");
        Serial.println(_name);
        subscribe("On");
        subscribe("Hue");
        subscribe("Saturation");
        subscribe("Brightness");
        add_to_homebridge();
    } else {
        Serial.println("Not Connected");
    }
}

void Light::add_to_homebridge() {
    Serial.print("Adding light to homebridge ");
    Serial.println(_name);
    char payload[128];
    sprintf(payload, "{ \"name\": \"%s\", \"Service\": \"Lightbulb\", \"Hue\": \"0\", \"Saturation\": \"100\", \"Brightness\": \"50\" }", _name.c_str());
    byte length = (byte)strlen(payload);
    if(!mqtt_client.publish("homebridge/to/add", payload, length)) {
        Serial.println("Failed");
    } else {
        Serial.println("Light added!");
    }
    sprintf(payload, "{\"name\": \"%s\", \"reachable\": true}", _name.c_str());
    length = (byte)strlen(payload);
    if(!mqtt_client.publish("homebridge/to/set/reachability", payload, length)) {
        Serial.println("Reachability restored");
    }
}

void Light::update() {
    _prog();
    FastLED.show();
    _count++;
}

void Light::turn_on() {
    if(!_onoff) {
        _color = CRGB::White;
        _onoff = 1;
        update();
    }
}

void Light::turn_off() {
    if(_onoff) {
        _color = CRGB::Black;
        _onoff = 0;
        update();
    }
}

void Light::toggle() {
    if (_onoff == 1) {
        turn_off();
        _onoff = 0;
    } else {
        turn_on();
        _onoff = 1;
    }
}

void Light::set_rgb(CRGB color) {
    _color = color;
    update();
}

void Light::set_hue(int val) {
    CHSV hsv_color = get_hsv();
    hsv_color.h = val;
    set_hsv(hsv_color);
    update();
}

void Light::set_brightness(int val) {
    CHSV hsv_color = get_hsv();
    hsv_color.v = val;
    set_hsv(hsv_color);
    update();
}

void Light::set_saturation(int val) {
    CHSV hsv_color = get_hsv();
    hsv_color.s = val;
    set_hsv(hsv_color);
    update();
}

void Light::set_hsv(int hue, int sat, int val) {
    _color = CHSV(hue, sat, val);
    update();
}

void Light::set_hsv(CHSV input_color) {
    _color = input_color;
    update();
}

CHSV Light::get_hsv() {
    return rgb2hsv_approximate(_color);
}

CRGB Light::get_rgb() {
    return _color;
}

const char* Light::get_name() {
    return _name.c_str();
}

// programs

void Light::_prog_solid() {
    for (int i=0; i<_num_leds; i++) {
        _leds[i] = _color;
    }
}

void Light::_prog_fade() {
    for(int i=0; i<_num_leds; i++) {
        _leds[i].fadeToBlackBy(20);
    }
}

void Light::_prog_chase() {
    _prog_fade();
    _leds[_count%_num_leds] = _color;
}