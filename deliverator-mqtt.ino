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
#define NUM_LEDS 23

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

// FastLED Vars
CRGB leds[NUM_LEDS];
int count;

#define L1_START 0
#define L1_END 2
#define L2_START 3
#define L2_END 12
#define L3_START 13
#define L3_END 22

void MQTT_connect();

void setup() {
    Serial.begin(115200);
    Serial.println(F("Starting up MQTT LED Controller"));
    delay(10);

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
            for(int i=L1_START;i<=L1_END;i++) {
                leds[i] = CHSV(0,0,String((char *)brightness_1.lastread).toInt());
            }
        }
        if(subscription == &brightness_2) {
            for(int i=L2_START;i<=L2_END;i++) {
                leds[i] = CHSV(0,0,String((char *)brightness_2.lastread).toInt());
            }
        }
        if(subscription == &brightness_3) {
            for(int i=L3_START;i<=L3_END;i++) {
                leds[i] = CHSV(0,0,String((char *)brightness_3.lastread).toInt());
            }
        }
        if(subscription == &color_1) {

        }
        if(subscription == &color_2) {

        }
        if(subscription == &color_3) {

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