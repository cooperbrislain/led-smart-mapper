#include "Arduino.h"
#include "PubSubClient.h"
#include "light.h"
#include "config.h"

void Light::subscribe(PubSubClient *mqtt_client) {
    char device[128];
    sprintf(device, "/%s/%s", DEVICE_NAME, _name.c_str());
    String feed = device;
    if (!mqtt_client->connected()) {
//        Serial.println("Not Connected");
    } else if(!mqtt_client->subscribe(feed.c_str())) {
//        Serial.print("Failed to subscribe to feed: ");
//        Serial.println(feed);
    } else {
//        Serial.print("Subscribed to feed: ");
//        Serial.println(feed);
    }
}

void Light::update() {
    (this->*_prog)(_params[1]||0);
    _count++;
}

void Light::turn_on() {
    #ifdef FADE
        _prog = &Light::_prog_fadein;
        _params[1] = FADE;
    #endif
    _onoff = 1;
    update();
}

void Light::turn_off() {
    if(_onoff) {
        #ifdef FADE
            _prog = &Light::_prog_fadeout;
            _params[1] = FADE;
        #else
            _onoff = 0;
        #endif
        update();
    }
}

void Light::set_on(int onoff) {
    _onoff = onoff;
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
}

void Light::set_hue(int val) {
    CHSV hsv_color = get_hsv();
    hsv_color.h = val;
    set_hsv(hsv_color);
}

void Light::set_brightness(int val) {
    CHSV hsv_color = get_hsv();
    hsv_color.v = min(val, 100);
    set_hsv(hsv_color);
}

void Light::set_saturation(int val) {
    CHSV hsv_color = get_hsv();
    hsv_color.s = min(val, 100);
    set_hsv(hsv_color);
}

void Light::set_hsv(int hue, int sat, int val) {
    _color = CHSV(hue, sat, val);
}

void Light::set_hsv(CHSV color) {
    _color = color;
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

int Light::get_param(int p) {
    return _params[p];
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
        *_leds[i] = fadeTowardColor(*_leds[i], _color, x);
        if (*_leds[i] != _color) still_fading = true;
    }
    if (!still_fading) _prog = &Light::_prog_solid;
    return 0;
}

int Light::_prog_fadeout(int x) {
    bool still_fading = false;
    for(int i=0; i<_num_leds; i++) {
        _leds[i]->fadeToBlackBy(x);
        if (*_leds[i]) still_fading = true;
    }
    if (!still_fading) _onoff = false;
    return 0;
}

int Light::_prog_chase(int x) {
    // params: 0: Chase Speed
    //         1: Fade Speed
    _prog_fade(_params[1]);
    *_leds[_count%_num_leds] = _color;
    return 0;
}

int Light::_prog_warm(int x) {
    if (_count%7 == 0) _prog_fade(10);

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

    int Light::_prog_artnet(int x) {
        for (int i=0; i<_num_leds; i++) {
           & _leds[i] = artnet_leds[_offset+i];
        }
    }
#endif

// Helper Functions

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