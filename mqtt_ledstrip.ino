#define MQTT_DEBUG
#include "SPI.h"
#include <Ethernet.h>
#include <EthernetClient.h>
#include <Dns.h>
#include <Dhcp.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "FastLED.h"

// Ethernet Defs

// MQTT defs
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "cbrislain"
#define AIO_KEY         "a95102c1e84b4edb82aea735a5e7f82c"

// FastLED defs
#define DATA_PIN 7
#define NUM_LEDS 51
#define NUM_LIGHTS 10

// Ethernet Vars
byte mac[]    = {  0xD3, 0x3D, 0xB4, 0xF3, 0xF3, 0x3D };
//IPAddress ip(192,168,100,98);
EthernetClient client;

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");

#define halt(s) { Serial.println(F( s )); while(1);  }

// FastLED Vars
CRGB leds[NUM_LEDS];
int count;



class Light {
    public:

        Light(String name, CRGB* leds, int num_leds);
        Light();

        void turn_on();
        void turn_off();
        void toggle();
        void set_brightness(int val);
        void set_hue(int val);
        void set_saturation(int val);
        void set_hsv(int hue, int sat, int val);

    private:
        CRGB* _leds;
        CRGB _color;
        int _num_leds;
        int _last_brightness;
        int _status;
        String _name;
        Adafruit_MQTT_Subscribe _sub_hue;
        Adafruit_MQTT_Subscribe _sub_brightness;
        Adafruit_MQTT_Subscribe _sub_status;

        void update();
};

Light::Light() {
    _color = CRGB::Black;
    _status = 0;
    _num_leds = 0;
    _leds = 0;
    _sub_hue = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/light/hue");
    _sub_brightness = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/light/brightness");
    _sub_status = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/light/status");
}

Light::Light(String name, CRGB* leds, int num_leds) {
    _color = CRGB::Black;
    _status = 0;
    _num_leds = num_leds;
    _leds = leds;
    _name = name;
    _sub_hue = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/" + _name + "/hue");
    _sub_brightness = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/" + _name + "/brightness");
    _sub_status = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/" + _name + "/status");
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

void Light::set_hue(int val) {
    _color = CHSV(val, 255, 255);
    update();
}

void Light::set_saturation(int val) {
    _color = CHSV(255, val, 255);
    update();
}

void Light::set_hsv(int hue, int sat, int val) {
    _color = CHSV(hue, sat, val);
    update();
}

void MQTT_connect();

Light lights[NUM_LIGHTS];

void setup() {
    Serial.begin(115200);
    Serial.println(F("Starting up MQTT LED Controller"));
    delay(10);
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
        FastLED.show();
    }

    // initialize lights;
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

    for (int i=0; i<10; i++){
        for (int i=0; i<NUM_LIGHTS; i++) {
            lights[i].turn_on();
            delay(50);
            lights[i].turn_off();
            delay(25);
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
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
        FastLED.show();
        delay(100);
    }
    for (int i=0; i<NUM_LIGHTS; i++) {
        mqtt.subscribe(&lights[i].get_sub("hue"));
        mqtt.subscribe(&lights[i].get_sub("brightness"));
        mqtt.subscribe(&lights[i].get_sub("status"));
    }
    for (int i=0; i<NUM_LIGHTS; i++) {
        lights[i].turn_off();
    }
    FastLED.show();
}

void loop() {
    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect
    // function definition further below.
    MQTT_connect();
    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(5000))) {
        if (subscription == &onoffbutton) {
            Serial.print(F("Got: "));
            Serial.println((char *)onoffbutton.lastread);
            if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
                for (int i=0;i<NUM_LEDS;i++) {
                    //leds[i] = CRGB::Black;
                }
            }
            if (strcmp((char *)onoffbutton.lastread, "ON") == 0){
                for (int i=0;i<NUM_LEDS;i++) {
                    //leds[i] = CRGB::White;
                }
            }
            FastLED.show();
        }
        if(subscription == &brightness_1) {
            //lights[0].color.val = String((char *)brightness_1.lastread).toInt();
        }
        if(subscription == &brightness_2) {
            //lights[1].color.val = String((char *)brightness_2.lastread).toInt();
        }
        if(subscription == &brightness_3) {
            //lights[2].color.val = String((char *)brightness_3.lastread).toInt();
        }
        if(subscription == &color_1) {
            //lights[1].color = rgb2hsv_approximate(CRGB(String((char *)color_1.lastread).toInt()));
        }
        if(subscription == &color_2) {

        }
        if(subscription == &color_3) {

        }
        for(int i=0; i<NUM_LIGHTS; i++) {
        }
        FastLED.show();
    }
}


void MQTT_connect() {
    int8_t ret;

    // Stop if already connected.
    if (mqtt.connected()) {
        return;
    }

    uint8_t retries = 5;

    Serial.print("Connecting to MQTT... ");

    while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
        count++;
        for(int i=0;i<3;i++) {
            if(i=count%3) {
                leds[i] = CRGB::Red;
            } else {
                leds[i] = CRGB::Black;
            }
        }
        FastLED.show();
        switch (ret) {
            case 1: Serial.println("Wrong protocol"); break;
            case 2: Serial.println("ID rejected"); break;
            case 3: Serial.println("Server unavailable"); break;
            case 4: Serial.println("Bad user/password"); break;
            case 5: Serial.println("Not authenticated"); break;
            case 6: Serial.println("Failed to subscribe"); break;
            default:
                Serial.print("Couldn't connect to server, code: ");
                Serial.println(ret);
                break;
            }
        Serial.println(mqtt.connectErrorString(ret));
        Serial.println("Retrying MQTT connection in 5 seconds...");
        mqtt.disconnect();
        delay(5000);  // wait 5 seconds
        retries--;
        if (retries == 0) {
            Serial.println("Failed after too many retries.");
            while (1);
        }
    }
    Serial.println("MQTT Connected!");
}