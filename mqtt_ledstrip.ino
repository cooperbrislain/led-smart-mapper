#define MQTT_DEBUG
#include "SPI.h"
#include <Ethernet.h>
#include <Dns.h>
#include <Dhcp.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include "config.h"
#include <ArduinoJson.h>


#define DATA_PIN 40
#define CLOCK_PIN 41
#define NUM_LEDS 134

#define NUM_LIGHTS 5

#define halt(s) { Serial.println(F( s )); while(1);  }

const char *wifi_ssid = WIFI_SSID;
const char *wifi_pass = WIFI_PASS;

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
        unsigned int _count;
        int _prog_solid();
        int _prog_chase();
        int _prog_fade();
        int _prog_warm();
        int _prog_lfo();
        int (Light::*_prog)();
        // void add_to_homebridge();
        void subscribe();
};

// Ethernet Vars
byte mac[] = { 0xDA, 0x3D, 0xB3, 0xF3, 0xF0, 0x3D };

const char* mqtt_server = MQTT_HOST;
const int mqtt_port = 1883;
const char* mqtt_username = MQTT_USER;
const char* mqtt_key = MQTT_PASS;

CRGB leds[NUM_LEDS];
Light lights[NUM_LIGHTS];

int speed = 500;

EthernetClient eth_client;
PubSubClient mqtt_client(eth_client);

void setup() {
    Serial.begin(115200);
    Serial.println("Starting up MQTT LED Controller");
    delay(10);
    // initialize fastled and test all lights

    FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR, DATA_RATE_MHZ(24)>(leds, NUM_LEDS);
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::White; // blink white
    }
    FastLED.show();
    delay(10);
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black; // go black first
    }
    FastLED.show();
    // for (int i=0; i<NUM_LEDS; i++) {
    //     leds[i] = CRGB::Blue; 
    //     FastLED.show();
    //     delay(10);
    // }
    mqtt_client.setServer(mqtt_server, mqtt_port);
    mqtt_client.setCallback(mqtt_callback);

    // initialize lights;
    // TODO: Make this based off of a config file
    //lights[0] = Light("right_cube", &leds[0], 10);
    lights[0] = Light("downlight", &leds[0], 57);
    lights[1] = Light("uplight", &leds[57], 57);
    lights[2] = Light("workstation", &leds[10], 20);
    lights[3] = Light("bench", &leds[40], 17);
    Ethernet.begin(mac);
    Serial.println(F("Connecting..."));
    if(!Ethernet.begin(mac)) {
        Serial.println(F("Ethernet configuration failed."));
        for(;;);
    }
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black; 
        FastLED.show();
        delay(10);
    }
    Serial.println(F("Ethernet configured via DHCP"));
    Serial.print("IP address: ");
    Serial.println(Ethernet.localIP());
    Serial.println();
    delay(150);
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
    delay(1000/speed);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, (char *)payload);
    payload[length] = '\0';
    char* tmp = strtok(topic,"/");
    char* name = strtok(NULL,"/");
    if (strcmp(tmp, "speed") == 0) {
        speed = atoi((char *)payload);
    } else if (strcmp(tmp, "lights") == 0) {
        for (int i=0; i<NUM_LIGHTS; i++) {
            if (strcmp(name, lights[i].get_name()) == 0) {
                if (doc.containsKey("On")) {
                    bool OnOff = doc["On"].as<bool>();
                    if(OnOff) {
                        Serial.println("Turning On");
                        lights[i].turn_on();
                    } else {
                        Serial.println("Turning Off");
                        lights[i].turn_off();
                    }
                }
                if (doc.containsKey("Hue")) {
                    int val = doc["Hue"].as<int>();
                    int hue = val * 17/24; // 255/360
                    Serial.print("Setting hue to ");
                    Serial.println(hue);
                    lights[i].set_hue(hue);
                }
                if (doc.containsKey("Brightness")) {
                    int val = doc["Brightness"].as<int>();
                    int bright = val * 51/20; // 255/100
                    Serial.print("Setting brightness to ");
                    Serial.println(bright);
                    lights[i].set_brightness(bright);
                }
                if (doc.containsKey("Saturation")) {
                    int val = doc["Saturation"].as<int>();
                    int sat = val * 51/20; // 255/100
                    Serial.print("Setting saturation to ");
                    Serial.println(sat);
                    lights[i].set_saturation(sat);
                }
                if (doc.containsKey("Program")) {
                    int val = doc["Program"];
                    lights[i].set_program(val);
                }
            }
        }
    }
}

void reconnect() {
  // Loop until we're reconnected
    while (!mqtt_client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (mqtt_client.connect("arduinoClient",mqtt_username,mqtt_key)) {
            Serial.println("connected");
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
    _prog = &Light::_prog_solid;
    _count = 0;
}

Light::Light(String name, CRGB* leds, int num_leds) {
    _color = CRGB::Black;
    _onoff = 0;
    _num_leds = num_leds;
    _leds = leds;
    _name = name;
    _prog = &Light::_prog_solid;
    _count = 0;
}

void Light::subscribe() {
    char tmp[128];
    sprintf(tmp, "/lights/%s", _name.c_str());
    String feed = tmp;
    if(!mqtt_client.subscribe(feed.c_str())) {
        Serial.print("Failed to subscribe to feed: ");
        Serial.println(feed);
    } else {
        Serial.print("Subscribed to feed: ");
        Serial.println(feed);
    }
}

void Light::initialize() {
    if (mqtt_client.connected()) {
        Serial.print("Subscribing to feeds for light:");
        Serial.println(_name);
        subscribe();
        // add_to_homebridge();
    } else {
        Serial.println("Not Connected");
    }
}

// void Light::add_to_homebridge() {
//     Serial.print("Adding light to homebridge ");
//     Serial.println(_name);
//     char payload[128];
//     sprintf(payload, "{ \"name\": \"%s\", \"Service\": \"Lightbulb\", \"Hue\": \"0\", \"Saturation\": \"100\", \"Brightness\": \"50\" }", _name.c_str());
//     byte length = (byte)strlen(payload);
//     if(!mqtt_client.publish("homebridge/to/add", payload, length)) {
//         Serial.println("Failed");
//     } else {
//         Serial.println("Light added!");
//     }
//     sprintf(payload, "{\"name\": \"%s\", \"reachable\": true}", _name.c_str());
//     length = (byte)strlen(payload);
//     if(!mqtt_client.publish("homebridge/to/set/reachability", payload, length)) {
//         Serial.println("Reachability restored");
//     }
// }

void Light::update() {
    if (_onoff == 1) {
        (this->*_prog)();
        FastLED.show();
        _count++;
        
    }
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
        for (int i=0; i<_num_leds;i++) {
            _leds[i] = CRGB::Black;
        }
        FastLED.show();
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

void Light::set_program(int prog_id) {
    Serial.print("Setting program to ");
    switch(prog_id) {
        case 0:
            _prog = &Light::_prog_solid;
            break;
        case 1:
            _prog = &Light::_prog_chase;
            break;
        case 2:
            _prog = &Light::_prog_fade;
            break;
        case 3:
            _prog = &Light::_prog_warm;
            break;
        case 4:
            _prog = &Light::_prog_lfo;
        default:
            break;
    }
}

const char* Light::get_name() {
    return _name.c_str();
}

// programs

int Light::_prog_solid() {
    for (int i=0; i<_num_leds; i++) {
        _leds[i] = _color;
    }
    return 0;
}

int Light::_prog_fade() {
    for(int i=0; i<_num_leds; i++) {
        _leds[i].fadeToBlackBy(20);
    }
    return 0;
}

int Light::_prog_chase() {
    _prog_fade();
    leds[_count%_num_leds] = _color;
    return 0;
}

int Light::_prog_warm() {
    _prog_fade();
    if (_count%25 == 0) {
        CRGB wc = _color;
        wc/=3;
        _leds[random(_num_leds)] += wc;
    }
    
    return 0;
}

int Light::_prog_lfo() {
    int wc = _color;
    wc%=(int)round((sin(_count*3.14/180)+0.5)*255);
    for(int i=0; i<_num_leds; i++) {
        _leds[i] = wc;
    }
}