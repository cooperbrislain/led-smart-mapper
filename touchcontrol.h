#ifndef MQTT_LEDSTRIP_TOUCHCONTROL_H
#define MQTT_LEDSTRIP_TOUCHCONTROL_H

#include "Arduino.h"
#include "config.h"

#define TOUCH_PIN T0

typedef void(*ControlFn)(int);

class TouchControl {
    public:
        TouchControl()
        {
            _pressFn = [](int val){ };
            _releaseFn = [](int val){ };
            _stilldownFn = [](int val){ };
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
            _releaseFn { [](int val){ } },
            _stilldownFn { [](int val){ } },
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
            _stilldownFn { stilldownFn },
            _releaseFn { releaseFn }
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
        int _pressed = 0;
        int _val;
        int _pin;
        int _threshold;
        ControlFn _pressFn;
        ControlFn _releaseFn;
        ControlFn _stilldownFn;
};

#endif //MQTT_LEDSTRIP_TOUCHCONTROL_H