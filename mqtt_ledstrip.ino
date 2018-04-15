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
#define NUM_LEDS 100
#define NUM_LIGHTS 5

// Ethernet Vars
byte mac[]    = {  0xD3, 0x3D, 0xB4, 0xF3, 0xF3, 0x3D };
//IPAddress ip(192,168,100,98);
EthernetClient client;

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");
Adafruit_MQTT_Subscribe color_1 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/color_1");
Adafruit_MQTT_Subscribe color_2 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/color_2");
Adafruit_MQTT_Subscribe color_3 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/color_3");
Adafruit_MQTT_Subscribe brightness_1 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/brightness_1");
Adafruit_MQTT_Subscribe brightness_2 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/brightness_2");
Adafruit_MQTT_Subscribe brightness_3 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/brightness_3");

#define halt(s) { Serial.println(F( s )); while(1);  }

class Light {
    public:
        CHSV color;

        Light(CRGB *leds);

        void turn_on();
        void turn_off();
        void toggle();
        void set_brightness();
        void set_hue();
        void set_saturation();
        void set_hsv();

    private:
        CRGB _leds;
        int _num_leds;
        int _offset;
        int _last_brightness;
        int _status;

        void update();
};

Light::Light(CRGB &leds, num_leds) {
    _color = CHSV(0,0,0);
    _status = 0;
    _num_leds = num_leds
    update();
}

void Light::update() {
    for (int i=offset; i<num_leds; i++) {
        _leds[i] = color;
    }
}

void Light::turn_on() {
    color = CHSV(color.h, color.s, 255);
    update();
}

void Light::turn_off() {
    color = CHSV(color.h, color.s, 0);
    update();
}

void Light::toggle() {
    if (status == 1) {
        turn_off();
    } else {
        turn_on();
    }
}

void Light::set_brightness(int val) {
    color = CHSV(color.h, color.s, val);
    update();
}

void Light::set_hue(int val) {
    color = CHSV(val, color.s, color.v);
    update();
}

void Light::set_saturation(int val) {
    color = CHSV(color.h, val, color.v);
    update();
}

void Light::set_hsv(int hue, int sat, int val) {
    color = CHSV(hue, sat, val);
    update();
}
// FastLED Vars
CRGB leds[NUM_LEDS];
Light lights[NUM_LIGHTS];
int count;



void MQTT_connect();

void setup() {
    Serial.begin(115200);
    Serial.println(F("Starting up MQTT LED Controller"));
    delay(10);
    lights[0].num_leds = 9;
    lights[1].num_leds = 9;
    lights[2].num_leds = 9;
    lights[3].num_leds = 3;
    lights[3].num_leds = 3;
    int offset = 0;
    for (int i=0;i<NUM_LIGHTS;i++) {
        lights[i].offset = offset;
        offset += lights[i].num_leds;
    }
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::White;
        FastLED.show();
        delay(100);
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
    mqtt.subscribe(&onoffbutton);
    mqtt.subscribe(&brightness_1);
    mqtt.subscribe(&brightness_2);
    mqtt.subscribe(&brightness_3);
    mqtt.subscribe(&color_1);
    mqtt.subscribe(&color_2);
    mqtt.subscribe(&color_3);
    for (int i=0; i<NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
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
                    leds[i] = CRGB::Black;
                }
            }
            if (strcmp((char *)onoffbutton.lastread, "ON") == 0){
                for (int i=0;i<NUM_LEDS;i++) {
                    leds[i] = CRGB::White;
                }
            }
            FastLED.show();
        }
        if(subscription == &brightness_1) {
            lights[0].color.val = String((char *)brightness_1.lastread).toInt();
        }
        if(subscription == &brightness_2) {
            lights[1].color.val = String((char *)brightness_2.lastread).toInt();
        }
        if(subscription == &brightness_3) {
            lights[2].color.val = String((char *)brightness_3.lastread).toInt();
        }
        if(subscription == &color_1) {
            lights[1].color = rgb2hsv_approximate(CRGB(String((char *)color_1.lastread).toInt()));
        }
        if(subscription == &color_2) {

        }
        if(subscription == &color_3) {

        }
        for(int i=0; i<NUM_LIGHTS; i++) {
            for(int j=lights[i].offset; j<lights[i].offset+lights[i].num_leds;j++) {
                leds[j] = lights[i].color;
            }
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