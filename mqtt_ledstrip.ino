#include "config.h"

#include <SPI.h>
#include <FastLED.h>

#ifndef NO_NETWORK
    #include <Dns.h>
    #include <Dhcp.h>
    #include <PubSubClient.h>
    #include <ArduinoJson.h>
#endif
#ifdef ARTNET
    #include <Artnet.h>
#endif

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
#ifdef TOUCH
    #define TOUCH_PIN T0
    #ifndef TOUCH_THRESHOLD
        #define TOUCH_THRESHOLD 50
    #endif
    int touch_value = 100;
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

#ifdef USE_WIFI
    const char *wifi_ssid = WIFI_SSID;
    const char *wifi_pass = WIFI_PASS;
#endif

#ifndef NO_NETWORK
    void mqtt_callback(char* topic, byte* payload, unsigned int length);

    #ifdef ARTNET
        void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data, IPAddress remoteIP);
    #endif
#endif

void reconnect();
void blink();
void blink_rainbow();
void blackout();
uint8_t nblendU8TowardU8(uint8_t cur, const uint8_t target, uint8_t x);
typedef void(*ControlFn)(int);

ControlFn default_pressFn = [](int val){
    Serial.print("default pressFn");
    Serial.println(val);
};

ControlFn default_releaseFn = [](int val){
    Serial.print("default releaseFn");
    Serial.println(val);
};

ControlFn default_stilldownFn = [](int val){
    Serial.print("default stilldownFn");
    Serial.println(val);
};

class TouchControl {
    public:
        TouchControl()
        {
            _pressFn = default_pressFn;
            _releaseFn = default_releaseFn;
            _stilldownFn = default_stilldownFn;
            _pin = TOUCH_PIN;
            _threshold = TOUCH_THRESHOLD;
            _name = "button";
        };
        TouchControl(
            String name,
            int pin,
            int threshold,
            ControlFn pressFn
        ) :
            _pressFn { pressFn },
            _releaseFn { default_releaseFn },
            _stilldownFn { default_stilldownFn },
            _pin { pin },
            _name { name },
            _threshold { threshold }
            { };
        TouchControl(
            String name,
            int pin,
            int threshold,
            ControlFn pressFn,
            ControlFn stilldownFn,
            ControlFn releaseFn
        ) :
            _name { name },
            _pin { pin },
            _threshold { threshold },
            _pressFn { pressFn },
            _releaseFn { releaseFn },
            _stilldownFn { stilldownFn }
        { };
        TouchControl(String name, int pin, int threshold);
        int  get_state();
        void set_press(ControlFn pressFn);
        void set_release(ControlFn releaseFn);
        void set_stilldown(ControlFn stilldownFn);
        bool is_pressed();
        void update();
    private:
        String _name;
        int    _pressed = 0;
        int    _val;
        int    _pin;
        int    _threshold;
        ControlFn _pressFn;
        ControlFn _releaseFn;
        ControlFn _stilldownFn;
};

class Light {
    public:
        Light(String name, CRGB* leds, int offset, int num_leds, int inverse=0);
        Light();
        Light(String name, CRGB** leds);
        const char* get_name();
        void turn_on();
        void turn_off();
        void blink();
        void toggle();
        void set_speed(int val);
        void set_hue(int val);
        void set_brightness(int val);
        void set_saturation(int val);
        void set_rgb(CRGB);
        void set_hsv(int hue, int sat, int val);
        void set_hsv(CHSV);
        void set_program(const char* program);
        void set_param(int p, int v);
        void set_params(int* params);
        CRGB get_rgb();
        CHSV get_hsv();
        #ifndef NO_NETWORK
            void initialize();
        #endif
        void update();

    private:
        CRGB** _leds;
        CRGB _color;
        int _params [NUM_PARAMS];
        int _speed;
        int _num_leds;
        int _offset;
        int _last_brightness;
        bool _onoff;
        String _name;
        unsigned int _count;
        unsigned int _index;
        int _prog_solid(int x);
        int _prog_chase(int x);
        int _prog_blink(int x);
        int _prog_fade(int x);
        int _prog_warm(int x);
        int _prog_xmas(int x);
        int _prog_lfo(int x);
        int _prog_fadeout(int x);
        int _prog_fadein(int x);
        int _prog_longfade(int x);
        int (Light::*_prog)(int x);
        #ifndef NO_NETWORK
            void subscribe();
            // void add_to_homebridge();
            #ifdef ARTNET 
                int _prog_artnet(int x); 
            #endif
        #endif
};

#ifdef USE_ETHERNET
    byte mac[] = ETH_MAC;
    byte broadcast[] = ETH_BROADCAST;
    byte ip[] = ETH_IP;
#else
    byte mac[6] = { };
    byte broadcast[4] = { };
    byte ip[4] = { };
#endif

CRGB leds[NUM_LEDS];

Light           lights[NUM_LIGHTS];
TouchControl    controls[NUM_CONTROLS];

#ifndef NO_NETWORK
    #ifdef ARTNET
        Artnet artnet;
        CRGB artnet_leds [NUM_LEDS];
        const int numberOfChannels = NUM_LEDS * 3;
        const int startUniverse = 0; 
        const int maxUniverses = numberOfChannels / 512 + ((numberOfChannels % 512) ? 1 : 0);
        bool universesReceived[maxUniverses];
        bool sendFrame = 1;
        int previousDataLength = 0;
    #endif

    #ifdef USE_ETHERNET
        EthernetClient net_client;
    #endif
    #ifdef USE_WIFI
        WiFiClient net_client;
    #endif

    const char* mqtt_server = MQTT_HOST;
    const int mqtt_port = 1883;
    const char* mqtt_username = MQTT_USER;
    const char* mqtt_key = MQTT_PASS;
    const char* mqtt_id = MQTT_CLID;

    PubSubClient mqtt_client(net_client);
#endif

int speed = GLOBAL_SPEED;

int count = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting up LED Controller");
    delay(10);
    FastLED.setBrightness(BRIGHTNESS_SCALE);
    #ifdef IS_APA102
        FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR, DATA_RATE_MHZ(24)>(leds, NUM_LEDS);
    #endif
    #ifdef IS_WS2801
        FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
    #endif

    blink();

    Serial.println("LEDs initialized");

    #ifdef LIGHTS
        lights = LIGHTS;
    #else
        /*
        lights[0] = Light("front", &leds[0], 0, 25); // FRONT
        lights[1] = Light("left", &leds[0], 25, 25, 1); // LEFT
        lights[2] = Light("right", &leds[0], 50, 25); // RIGHT
        lights[3] = Light("rear", &leds[0], 75, 25); // REAR
        */
        /* Test setup */
        lights[0] = Light("front", &leds[0], 6, 8);
        lights[1] = Light("left", &leds[0], 2, 4);
        lights[2] = Light("right", &leds[0], 14, 4);
        CRGB* rearLeds[4] = { &leds[0], &leds[1], &leds[18], &leds[19] };
        lights[3] = Light("rear", rearLeds);

    #endif

    for (Light light : lights) {
        light.blink();
    }

    Serial.println("Light Mapping Initialized");

    #ifdef TOUCH
        controls[0] = TouchControl("left", T0, 20,
            [](int val){
                Serial.println("Toggle");
                lights[0].toggle();
            },
            [](int val){ },
            [](int val){ }
            );
    #endif

    #ifndef NO_NETWORK
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

        mqtt_client.setServer(mqtt_server, mqtt_port);
        mqtt_client.setCallback(mqtt_callback);

        broadcast[0] = ip[0] = localip[0];
        broadcast[1] = ip[1] = localip[1];
        broadcast[2] = ip[2] = localip[2];
        ip[3] = localip[3];
        broadcast[3] = 0xff;
        for (int i=0; i<NUM_LEDS; i++) {
            leds[i] = CRGB::Black; 
        }
        FastLED.show();
        delay(100);
        Serial.println(F("Network configured via DHCP"));
        Serial.print("IP address: ");
        Serial.println(localip);
        #ifdef ARTNET
            Serial.println("Initializing Artnet");
            artnet.begin(mac, ip);
            artnet.setBroadcast(broadcast);
            artnet.setArtDmxCallback(onDmxFrame);
            Serial.println("Artnet Initialized");
        #endif

        blink_rainbow();
        delay(100);
        blackout();
        Serial.println("Setup complete, starting...");
    #endif

    delay(150);
}

void loop() {
    #ifndef NO_NETWORK
        if (!mqtt_client.connected()) {
            Serial.println("Reconnecting...");
            reconnect();
        }
        mqtt_client.loop();
        #ifdef ARTNET
            artnet.read();
        #endif
    #endif
    #ifdef TOUCH
        for (int i=0; i<NUM_CONTROLS; i++) controls[i].update();
    #endif
    for (Light light : lights) light.update();
    FastLED.show();
    count++;
    delay(1000/speed);
}

void blink() {
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::White;
    }
    FastLED.show();
    delay(25);
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
}

void blink_rainbow() {
    CHSV color;
    for (int t=0; t<100; t++) {
        color = CHSV((t*5)%255, 255, 100);
        for (int i=0; i<NUM_LEDS; i++) {
            leds[i] = color;
        }
        FastLED.show();
        delay(10);
    }
}

void blackout() {
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
    }
    FastLED.show();
}

#ifndef NO_NETWORK

    void mqtt_callback(char* topic, byte* payload, unsigned int length) {
        DynamicJsonDocument json(1024);
        deserializeJson(json, (char *)payload);
        payload[length] = '\0';
        char* device = strtok(topic,"/");
        char* name = strtok(NULL,"/");
        if (strcmp(device, DEVICE_NAME) == 0
            || strcmp(device, "global") == 0
            || strcmp(device, "all" == 0)) {
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
                        if (device == "global") {
                            Serial.print("Setting speed to ");
                            Serial.println(val);
                            speed = val;
                        } else {
                            lights[i].set_speed(val);
                        }
                    }
                    if (json.containsKey("Params")) {
                        Serial.println("Setting params");
                        JsonArray params = json["Params"].as<JsonArray>();
                        for (int p=0; p<sizeof(params); p++) {
                            int v = params[i].as<int>();
                            Serial.print("Setting param ");
                            Serial.print(p);
                            Serial.print(" to ");
                            Serial.print(v);
                            lights[i].set_param(p, v);
                        }
                    }
                    if (json.containsKey("Param")) {
                        int p = json["Param"]["p"];
                        int v = json["Param"]["v"];
                        lights[i].set_param(p, v);
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
                char feed[128];
                sprintf(feed, "/%s/global", DEVICE_NAME);
                mqtt_client.subscribe(feed);
                sprintf(feed, "/%s/all", DEVICE_NAME);
                mqtt_client.subscribe(feed);
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

#endif

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

#ifdef ARTNET
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
#endif

// TouchControl member functions

TouchControl::TouchControl(String name, int pin, int threshold) {
    _name = name;
    _pin = pin;
    _threshold = threshold;
    _pressed = 0;
    _val = 0;
    _pressFn = default_pressFn;
    _releaseFn = default_releaseFn;
    _stilldownFn = default_stilldownFn;
}

void TouchControl::update() {
    int val = touchRead(_pin);
    if (!_pressed && val <= _threshold) {
        _pressed = 1;
        _pressFn(val);
    } else if (_pressed && val <= _threshold) {
        _stilldownFn(val);
    } else if (_pressed && val > _threshold) {
        _pressed = 0;
        _releaseFn(val);
    }
}

int TouchControl::get_state() {
    return _val;
}

bool TouchControl::is_pressed() {
    return _pressed!=0;
}

void TouchControl::set_press(ControlFn pressFn) {
    _pressFn = pressFn;
}

void TouchControl::set_release(ControlFn releaseFn) {
    _releaseFn = releaseFn;
}

void TouchControl::set_stilldown(ControlFn stilldownFn) {
    _stilldownFn = stilldownFn;
}

// Light member functions

Light::Light() {
    _color = CRGB::White;
    _onoff = 0;
    _num_leds = 0;
    _speed = 1;
    _name = "light";
    _prog = &Light::_prog_solid;
    _count = 0;
}

Light::Light(String name, CRGB* leds, int offset, int num_leds, int inverse) {
    _color = CRGB::White;
    _onoff = 0;
    _num_leds = num_leds;
    _speed = 1;
    _leds = new CRGB*[num_leds];
//    Serial.print("Mapping Light ");
//    Serial.println(name);
    for (int i=0; i<num_leds; i++) {
       _leds[i] = inverse? &leds[offset+num_leds-i-1] : &leds[offset+i];
    }
    _offset = offset;
    _name = name;
    _prog = &Light::_prog_solid;
    _count = 0;
}

Light::Light(String name, CRGB** leds) {
    _color = CRGB::White;
    _name = name;
    _offset = 0;
    _onoff = 0;
    _num_leds = sizeof(leds);
    _leds = new CRGB*[_num_leds];
    for (int i=0; i<sizeof(leds); i++) {
        _leds[i] = leds[i];
    }
    _prog = &Light::_prog_solid;
    _count = 0;
}

#ifndef NO_NETWORK

    void Light::subscribe() {
        char device[128];
        sprintf(device, "/%s/%s", DEVICE_NAME, _name.c_str());
        String feed = device;
        if(!mqtt_client.subscribe(feed.c_str())) {
            Serial.print("Failed to subscribe to feed: ");
            Serial.println(feed);
        } else {
//            Serial.print("Subscribed to feed: ");
//            Serial.println(feed);
        }
    }

    void Light::initialize() {
        if (mqtt_client.connected()) {
//            Serial.print("Subscribing to feeds for light:");
//            Serial.println(_name);
            subscribe();
            // add_to_homebridge();
        } else {
            Serial.println("Not Connected");
        }
    }

#endif

void Light::update() {
    if (_onoff == 1 && count%_speed == 0) {
        (this->*_prog)(_params[0]);
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

void Light::blink() {
    for (int i=0; i<_num_leds; i++) {
        *(CRGB*)_leds[i] = CRGB::White;
    }
    update();
    delay(25);
    for (int i=0; i<_num_leds; i++) {
        *(CRGB*)_leds[i] = CRGB::Black;
    }
    update();
}

void Light::toggle() {
    Serial.println(_onoff);
    if (_onoff) {
        Serial.println("OFF");
        turn_off();
    } else {
        Serial.println("ON");
        turn_on();
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
    hsv_color.v = min(val, 100);
    set_hsv(hsv_color);
    update();
}

void Light::set_saturation(int val) {
    CHSV hsv_color = get_hsv();
    hsv_color.s = min(val, 100);
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
    if (strcmp(program, "solid")==0) _prog = &Light::_prog_solid;
    if (strcmp(program, "chase")==0) {
        _prog = &Light::_prog_chase;
        _params[1] = _params[1]? _params[1] : 35;
    }
    if (strcmp(program, "fade")==0)  _prog = &Light::_prog_fade;
    if (strcmp(program, "blink")==0) _prog = &Light::_prog_blink;
    if (strcmp(program, "warm")==0) {
        _prog = &Light::_prog_warm;
        _params[0] = 50;
    }
    if (strcmp(program, "lfo")==0) _prog = &Light::_prog_lfo;
    #ifdef ARTNET
        if (strcmp(program, "artnet")==0) _prog = &Light::_prog_artnet;
    #endif
}

void Light::set_params(int* params) {
    for (int i=0; i<sizeof(params); i++) {
        _params[i] = params[i];
    }
}

void Light::set_param(int p, int v) {
    _params[p] = v;
}

void Light::set_speed(int s) {
    _speed = s;
}

const char* Light::get_name() {
    return _name.c_str();
}

// programs

int Light::_prog_solid(int x) {
    for (int i=0; i<_num_leds; i++) {
        *_leds[i] = _color;
    }
    return 0;
}

int Light::_prog_fade(int x) {
    for(int i=0; i<_num_leds; i++) {
        _leds[i]->fadeToBlackBy(x);
    }
    return 0;
}

int Light::_prog_fadein(int x) {
    bool still_fading = false;
    for(int i=0; i<_num_leds; i++) {
        *_leds[i] = fadeTowardColor(*_leds[i], _color, 2);
        if (*_leds[i] != _color) still_fading = true;
    }
    if (!still_fading) _prog = &Light::_prog_solid;
    return 0;
}

int Light::_prog_fadeout(int x) {
    bool still_fading = false;
    for(int i=0; i<_num_leds; i++) {
        _leds[i]->fadeToBlackBy(2);
        if (*_leds[i]) still_fading = true;
    }
    if (!still_fading) _onoff = false;
    return 0;
}

int Light::_prog_chase(int x) {
    // params: 1: Chase Speed
    //         1: Fade Speed
    _prog_fade(_params[1]);
    *_leds[_count%_num_leds] = _color;
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
    *_leds[_index] += _color;
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
    *_leds[_index] += _color;
    return 0;
}

int Light::_prog_lfo(int x) {
    int wc = _color;
    wc%=(int)round((sin(_count*3.14/180)+0.5)*255);
    for(int i=0; i<_num_leds; i++) {
        *_leds[i] = wc;
    }
    return 0;
}

int Light::_prog_longfade(int x) {
    bool still_fading = false;
    if(_count%10 == 0) {
        for(int i=0; i<_num_leds; i++) {
            _leds[i]->fadeToBlackBy(1);
            if (*_leds[i]) still_fading = true;
        }
        if (!still_fading) _onoff = false;
    }
    return 0;
}

int Light::_prog_blink(int x) {
    _prog_fade(25);
    if (!x) x = 25;
    if (_count%x == 0) {
        Serial.println("blink");
        for(int i=0; i<_num_leds; i++) {
            *_leds[i] = _color;
        }
    }
    return 0;
}

#ifdef ARTNET
    int Light::_prog_artnet(int x) {
        for (int i=0; i<_num_leds; i++) {
           & _leds[i] = artnet_leds[_offset+i];
        }
    }
#endif