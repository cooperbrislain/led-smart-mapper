#include "config.h"

#include "defs.h"

#include <SPI.h>
#include <FastLED.h>

#include "light.h"
#include "touchcontrol.h"

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
int speed = GLOBAL_SPEED;
int count = 0;

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

// SETUP

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
        lights = LIGHTS; // this doesn't work
    #else
    #ifdef BUMP_LED
        lights[0] = Light("bump", &leds[0], 0, 1);
    #endif
//        lights[1] = Light("front", &leds[1], 0, 25); // FRONT
//        lights[2] = Light("left", &leds[1], 25, 25, 25); // LEFT
//        lights[3] = Light("right", &leds[1], 50, 25); // RIGHT
//        lights[4] = Light("rear", &leds[1], 75, 25); // REAR
        /* Test setup */
        lights[1] = Light("front", &leds[1], 12, 13);
        lights[2] = Light("left", &leds[1], 0, 10);
        lights[3] = Light("right", &leds[1], 25, 10);
        CRGB* rearLeds[4] = { &leds[10], &leds[11], &leds[23], &leds[24] };
        lights[4] = Light("rear", rearLeds);

        lights[2].set_program("chase");
        lights[2].set_rgb(CRGB::Orange);
        lights[3].set_program("chase");
        lights[3].set_rgb(CRGB::Orange);
        lights[4].set_rgb(CRGB::Red);
    #endif

    for (Light light : lights) {
        light.blink();
    }

    Serial.println("Light Mapping Initialized");

    #ifdef TOUCH
        controls[0] = TouchControl("left", T1, TOUCH_THRESHOLD,
            [](int val) { lights[2].turn_on(); },
            [](int val) { },
            [](int val) { lights[2].turn_off(); }
        );
        controls[1] = TouchControl("right", T0, TOUCH_THRESHOLD,
            [](int val) { lights[3].turn_on(); },
            [](int val) { },
            [](int val) { lights[3].turn_off(); }
        );
        controls[2] = TouchControl("Green", T4, TOUCH_THRESHOLD,
            [](int val) { lights[1].turn_on(); },
            [](int val) { },
            [](int val) { lights[1].turn_off(); }
        );
        controls[3] = TouchControl("Red", T3, TOUCH_THRESHOLD,
            [](int val) { lights[4].turn_on(); },
            [](int val) { },
            [](int val) { lights[4].turn_off(); }
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

// MAIN LOOP

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
    for (int i=0; i<NUM_LIGHTS; i++) lights[i].update();
    FastLED.show();
    count++;
    Serial.println("======================LOOP======================");
    #ifdef SLOW
        delay(SLOW);
    #endif
    delay(1000/speed);
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
                    lights[i].subscribe(&mqtt_client);
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

// LED FUNCTIONS

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
