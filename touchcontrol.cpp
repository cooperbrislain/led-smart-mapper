#include "Arduino.h"
#include "touchcontrol.h"
#include "config.h"

TouchControl::TouchControl(String name, int pin, int threshold) {
    _name = name;
    _pin = pin;
    _threshold = threshold;
    _pressed = 0;
    _val = 0;
    _pressFn = [](int val) { };
    _releaseFn = [](int val) { };
    _stilldownFn = [](int val) { };
}

void TouchControl::update() {
    int val = touchRead(_pin);
    #ifdef DEBUG
        Serial.print("pin ");
        Serial.print(_pin);
        Serial.print(": ");
        Serial.println(val);
    #endif
    if (val <= _threshold) {
        _pressed++;
        if (_pressed == 10) _pressFn(val);
        if (_pressed > 20) _stilldownFn(val);
    } else {
        if (_pressed > 5) _releaseFn(val);
        _pressed = 0;
    }
}

int TouchControl::get_state() {
    int val = touchRead(_pin);
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


