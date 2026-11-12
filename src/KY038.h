#ifndef KY038_H
#define KY038_H

#include <Arduino.h>
#include <math.h>

class KY038 {

public:

    KY038(int pin) : _pin(pin) {
        pinMode(_pin, INPUT);
    }

    float get_value() {

        int minValue = 1023;
        int maxValue = 0;

        for (int i = 0; i < 100; i++) {

            int value = analogRead(_pin);

            if (value < minValue)
                minValue = value;

            if (value > maxValue)
                maxValue = value;

            delayMicroseconds(500);
        }

        float result = exp(maxValue - minValue);

        if (result > 65535) {
            result = 65535;
        }

        return result;
    }

private:

    int _pin;
};

#endif