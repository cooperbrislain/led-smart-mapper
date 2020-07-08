#ifndef MQTT_LEDSTRIP_LIGHT_H
#define MQTT_LEDSTRIP_LIGHT_H

#include <Arduino.h>
#include <SPI.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include "config.h"

class Light {
    public:
        Light() :
            _color { CRGB::White },
            _onoff { 0 },
            _num_leds { 0 },
            _name { "light" },
            _prog { &Light::_prog_solid },
            _count { 0 }
        { };
        Light(String name, CRGB* leds, int offset, int num_leds, int inverse=0) :
            _name { name },
            _num_leds { num_leds },
            _color { CRGB::White },
            _onoff { 0 },
            _count { 0 },
            _speed { 1 },
            _offset { offset },
            _prog { &Light::_prog_solid }
        {
            _leds = new CRGB*[num_leds];
            for (int i=0; i<num_leds; i++) {
                _leds[i] = inverse? &leds[offset+num_leds-i-1] : &leds[offset+i];
            }
        };
        Light(String name, CRGB** leds) :
            _name { name },
            _color { CRGB::White },
            _count { 0 },
            _offset { 0 },
            _onoff { 0 },
            _num_leds { sizeof(leds) },
            _prog { &Light::_prog_solid }
        {
            _leds = new CRGB*[_num_leds];
            for (int i=0; i<sizeof(leds); i++) {
                _leds[i] = leds[i];
            }
        };
        const char* get_name();
        void turn_on();
        void turn_off();
        void set_on(int onoff);
        void blink();
        void toggle();
        void set_hue(int val);
        void set_brightness(int val);
        void set_saturation(int val);
        void set_rgb(CRGB);
        void set_hsv(int hue, int sat, int val);
        void set_hsv(CHSV);
        void set_program(const char* program);
        void set_param(int p, int v);
        void set_params(int* params);
        int get_param(int p);
        CRGB get_rgb();
        CHSV get_hsv();
        void update();
        void subscribe(PubSubClient *mqtt_client);
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
        // void add_to_homebridge();
        #ifdef ARTNET
            int _prog_artnet(int x);
        #endif
};

uint8_t nblendU8TowardU8(uint8_t cur, const uint8_t target, uint8_t x);
CRGB fadeTowardColor(CRGB cur, CRGB target, uint8_t x);

#endif //MQTT_LEDSTRIP_LIGHT_H
