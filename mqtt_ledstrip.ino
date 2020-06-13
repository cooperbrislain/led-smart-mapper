#include "config.h"

#include <Dns.h>
#include <SPI.h>
#include <Dhcp.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <Artnet.h>

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
#ifdef USE_WIFI
    #include <WiFi.h>
#endif
#ifdef USE_ETHERNET
    #include <Ethernet.h>
#endif
#ifndef DEVICE_NAME
    #define DEVICE_NAME "led-bridge"
#endif

#ifndef NUM_LEDS
    #define NUM_LEDS 97
#endif
#ifndef NUM_LIGHTS
    #define NUM_LIGHTS 3
#endif
#ifndef BRIGHTNESS_SCALE
    #define BRIGHTNESS_SCALE 50
#endif

#define halt(s) { Serial.println(F( s )); while(1);  }

#ifdef USE_WIFI
    const char *wifi_ssid = WIFI_SSID;
    const char *wifi_pass = WIFI_PASS;
#endif

void mqtt_callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP);

class Light {
    public:
        Light(String name, CRGB* leds, int offset, int num_leds);
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
        void set_program(const char* program);
        CRGB get_rgb();
        CHSV get_hsv();
        void initialize();
        void update();

    private:
        int _params [3];
        CRGB* _leds;
        CRGB _color;
        int _num_leds;
        int _offset;
        int _last_brightness;
        bool _onoff;
        String _name;
        unsigned int _count;
        unsigned int _index;
        int _prog_solid(int x);
        int _prog_chase(int x);
        int _prog_fade(int x);
        int _prog_warm(int x);
        int _prog_xmas(int x);
        int _prog_lfo(int x);
        int _prog_fadeout(int x);
        int _prog_fadein(int x);
        int _prog_longfade(int x);
        int _prog_artnet(int x);
        int (Light::*_prog)(int x);
        // void add_to_homebridge();
        void subscribe();
};

// Ethernet Vars
byte mac[] = ETH_MAC;
byte broadcast[] = ETH_BROADCAST;
byte ip[] = ETH_IP;

const char* mqtt_server = MQTT_HOST;
const int mqtt_port = 1883;
const char* mqtt_username = MQTT_USER;
const char* mqtt_key = MQTT_PASS;
const char* mqtt_id = MQTT_CLID;

CRGB leds[NUM_LEDS];
Light lights[NUM_LIGHTS];
Artnet artnet;
CRGB artnet_leds [NUM_LEDS];

const int numberOfChannels = NUM_LEDS * 3;
const int startUniverse = 0; 
const int maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
bool universesReceived[maxUniverses];
bool sendFrame = 1;
int previousDataLength = 0;

int speed = 500;

#ifdef USE_ETHERNET
    EthernetClient net_client;
#endif
#ifdef USE_WIFI
    WiFiClient net_client;
#endif
PubSubClient mqtt_client(net_client);

void setup() {
    Serial.begin(115200);
    Serial.println("Starting up MQTT LED Controller");
    delay(10);
    // initialize fastled and test all lights
    FastLED.setBrightness(BRIGHTNESS_SCALE);
    #ifdef IS_APA102
        FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR, DATA_RATE_MHZ(24)>(leds, NUM_LEDS);
    #endif
    #ifdef IS_WS2801
        FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    #endif
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::White; // blink white
    }
    FastLED.show();
    delay(10);
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black; // go black first
    }
    FastLED.show();
    mqtt_client.setServer(mqtt_server, mqtt_port);
    mqtt_client.setCallback(mqtt_callback);
    // initialize lights;
    // TODO: Make this based off of a config file

    // lights[0] = Light("downlight", &leds[0], 10, 57);
    // lights[1] = Light("uplight", &leds[0], 67, 57);
    // lights[2] = Light("workstation", &leds[0], 20, 20);
    // lights[3] = Light("bench", &leds[0], 50, 17);
    // lights[4] = Light("rightcube", &leds[0], 0, 10);

    // lights[0] = Light("shelf", &leds[0],0,15);
    // lights[1] = Light("left", &leds[0],0,5);
    // lights[2] = Light("center", &leds[5],5,5);
    // lights[3] = Light("right", &leds[10],10,5);

    #ifndef LIGHTS
        lights[0] = Light("light", &leds[0], 0, NUM_LEDS);
    #endif


    #ifdef USE_ETHERNET
        Serial.println(F("Connecting to Ethernet..."));
        while (!Ethernet.begin(mac)) {
            Serial.println(F("Ethernet configuration failed."));
            delay(500);
        }
        IPAddress localip = Ethernet.localIP();
    #endif
    #ifdef USE_WIFI
        WiFi.begin(wifi_ssid, wifi_pass);
        while (WiFi.status() != WL_CONNECTED) {
            Serial.println("Connecting to WiFi...");
            delay(500);
        }
        IPAddress localip = WiFi.localIP();
    #endif
    broadcast[0] = ip[0] = localip[0];
    broadcast[1] = ip[1] = localip[1];
    broadcast[2] = ip[2] = localip[2];
    ip[3] = localip[3];
    broadcast[3] = 0xff;
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black; 
        FastLED.show();
        delay(10);
    }
    Serial.println(F("Network configured via DHCP"));
    Serial.print("IP address: ");
    Serial.println(localip);
    Serial.println("Initializing Artnet");
    artnet.begin(mac, ip);
    artnet.setBroadcast(broadcast);
    artnet.setArtDmxCallback(onDmxFrame);
    Serial.println("Artnet Initialized");
    delay(150);
}

void loop() {
    if (!mqtt_client.connected()) {
        Serial.println("Reconnecting...");
        reconnect();
    }
    mqtt_client.loop();
    artnet.read();
    for(int i=0; i<NUM_LIGHTS; i++) {
        lights[i].update();
    }
    delay(1000/speed);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    DynamicJsonDocument json(1024);
    deserializeJson(json, (char *)payload);
    payload[length] = '\0';
    char* tmp = strtok(topic,"/");
    char* name = strtok(NULL,"/");
    if (strcmp(tmp, DEVICE_NAME) == 0) {
        for (int i=0; i<NUM_LIGHTS; i++) {
            if (strcmp(name, lights[i].get_name()) == 0) {
                if (json.containsKey("On")) {
                    bool OnOff = json["On"].as<bool>();
                    if(OnOff) {
                        Serial.println("Turning On");
                        lights[i].turn_on();
                    } else {
                        Serial.println("Turning Off");
                        lights[i].turn_off();
                    }
                }
                if (json.containsKey("Hue")) {
                    int val = json["Hue"].as<int>();
                    int hue = val * 17/24; // 255/360
                    Serial.print("Setting hue to ");
                    Serial.println(hue);
                    lights[i].set_hue(hue);
                }
                if (json.containsKey("Brightness")) {
                    int val = json["Brightness"].as<int>();
                    int bright = val * 51/20; // 255/100
                    Serial.print("Setting brightness to ");
                    Serial.println(bright);
                    lights[i].set_brightness(bright);
                }
                if (json.containsKey("Saturation")) {
                    int val = json["Saturation"].as<int>();
                    int sat = val * 51/20; // 255/100
                    Serial.print("Setting saturation to ");
                    Serial.println(sat);
                    lights[i].set_saturation(sat);
                }
                if (json.containsKey("Program")) {
                    const char* program = json["Program"];
                    lights[i].set_program(program);
                }
                if (json.containsKey("Speed")) {
                    int val = json["Speed"].as<int>();
                    Serial.print("Setting speed to ");
                    Serial.println(val);
                    speed = val;
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
        if (mqtt_client.connect(mqtt_id,mqtt_username,mqtt_key)) {
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

CRGB fadeTowardColor(CRGB cur, CRGB target, uint8_t x) {
    CRGB newc;
    newc.red = nblendU8TowardU8(cur.red, target.red, x);
    newc.green = nblendU8TowardU8(cur.green, target.green, x);
    newc.blue = nblendU8TowardU8(cur.blue, target.blue, x);
    return newc;
}

uint8_t nblendU8TowardU8(uint8_t cur, const uint8_t target, uint8_t x) {
    uint8_t newc;
    if (cur == target) return newc = cur;
    if (cur < target) {
        uint8_t delta = target - cur;
        delta = scale8_video(delta, x);
        newc = cur + delta;
    } else {
        uint8_t delta = cur - target;
        delta = scale8_video(delta, x);
        newc = cur - delta;
    }
    return newc;
}

// Artnet

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP) {
    sendFrame = 1;

    if ((universe - startUniverse) < maxUniverses)
    universesReceived[universe - startUniverse] = 1;

    for (int i = 0 ; i < maxUniverses ; i++) {
        if (universesReceived[i] == 0) {
            sendFrame = 0;
            break;
        }
    }
    
    for (int i = 0; i < length / 3; i++) {
        int led = i + (universe - startUniverse) * (previousDataLength / 3);
        if (led < NUM_LEDS) {
            artnet_leds[led] = CRGB(data[i*3], data[i*3+1], data[i*3+2]);
        }
    }
    previousDataLength = length;

    if (sendFrame) {
        memset(universesReceived, 0, maxUniverses);
    }
}


// Light member functions

Light::Light() {
    _color = CRGB::White;
    _onoff = 0;
    _num_leds = 0;
    _leds = 0;
    _name = "light";
    _prog = &Light::_prog_solid;
    _count = 0;
    _params[0] = 50;
}

Light::Light(String name, CRGB* leds, int offset, int num_leds) {
    _color = CRGB::White;
    _onoff = 0;
    _num_leds = num_leds;
    _leds = &leds[offset];
    _offset = offset;
    _name = name;
    _prog = &Light::_prog_solid;
    _count = 0;
}

void Light::subscribe() {
    char tmp[128];
    sprintf(tmp, "/%s/%s", DEVICE_NAME, _name.c_str());
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

void Light::update() {
    if (_onoff == 1) {
        (this->*_prog)(_params[0]);
        FastLED.show();
        _count++;
        
    }
}

void Light::turn_on() {
    _prog = &Light::_prog_fadein;
    _onoff = 1;
    update();
}

void Light::turn_off() {
    if(_onoff) {
         _prog = &Light::_prog_fadeout;
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

void Light::set_hsv(CHSV color) {
    _color = color;
    update();
}

CHSV Light::get_hsv() {
    return rgb2hsv_approximate(_color);
}

CRGB Light::get_rgb() {
    return _color;
}

void Light::set_program(const char* program) {
    Serial.print("Setting program to ");
    Serial.println(program);
    if (strcmp(program, "solid")==0) _prog = &Light::_prog_solid;
    if (strcmp(program, "chase")==0) _prog = &Light::_prog_chase;
    if (strcmp(program, "fade")==0) _prog = &Light::_prog_fade;
    if (strcmp(program, "warm")==0) {
        _prog = &Light::_prog_warm;
        _params[0] = 50;
    }
    if (strcmp(program, "lfo")==0) _prog = &Light::_prog_lfo;
    if (strcmp(program, "artnet")==0) _prog = &Light::_prog_artnet;
}

const char* Light::get_name() {
    return _name.c_str();
}

// programs

int Light::_prog_solid(int x) {
    for (int i=0; i<_num_leds; i++) {
        _leds[i] = _color;
    }
    return 0;
}

int Light::_prog_fade(int x) {
    for(int i=0; i<_num_leds; i++) {
        _leds[i].fadeToBlackBy(x);
    }
    return 0;
}

int Light::_prog_fadein(int x) {
    bool still_fading = false;
    for(int i=0; i<_num_leds; i++) {
        _leds[i] = fadeTowardColor(_leds[i], _color, 1);
        if (_leds[i] != _color) still_fading = true;
    }
    if (!still_fading) _prog = &Light::_prog_solid;
    return 0;
}

int Light::_prog_fadeout(int x) {
    bool still_fading = false;
    for(int i=0; i<_num_leds; i++) {
        _leds[i].fadeToBlackBy(1);
        if (_leds[i]) still_fading = true;
    }
    if (!still_fading) _onoff = false;
    return 0;
}

int Light::_prog_chase(int x) {
    _prog_fade(6);
    leds[_count%_num_leds] = _color;
    return 0;
}

int Light::_prog_warm(int x) {
    if (_count%7 == 0) _prog_fade(1);
    
    if (_count%11 == 0) {
        _index = random(_num_leds);
        CHSV wc = rgb2hsv_approximate(_color);
        wc.h = wc.h + random(11)-5;
        wc.s = random(128)+128;
        wc.v &=x;
        _color = wc;
    }
    _leds[_index] += _color;
    return 0;
}

int Light::_prog_xmas(int x) {
    if (_count%7 == 0) _prog_fade(1);
    
    if (_count%11 == 0) {
        _index = random(_num_leds);
        CHSV wc = rgb2hsv_approximate(_color);
        wc.h = wc.h + random(11)-5;
        wc.s = random(128)+128;
        wc.v &=x;
        _color = wc;
    }
    _leds[_index] += _color;
    return 0;
}

int Light::_prog_lfo(int x) {
    int wc = _color;
    wc%=(int)round((sin(_count*3.14/180)+0.5)*255);
    for(int i=0; i<_num_leds; i++) {
        _leds[i] = wc;
    }
}

int Light::_prog_longfade(int x) {
    bool still_fading = false;
    if(_count%10 == 0) {
        for(int i=0; i<_num_leds; i++) {
            _leds[i].fadeToBlackBy(1);
            if (_leds[i]) still_fading = true;
        }
        if (!still_fading) _onoff = false;
    }
    return 0;
}

int Light::_prog_artnet(int x) {
    for (int i=0; i<_num_leds; i++) {
        _leds[i] = artnet_leds[_offset+i];
    }
}