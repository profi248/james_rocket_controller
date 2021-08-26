// https://github.com/KleistRobotics/Pico-Servo
// Control a servo by degrees or millis

#include <cstdio>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define MAX_MILLIS  2700
#define MIN_MILLIS  700
#define MAX_DEGREES 180
#define MIN_DEGREES 0

float clockDiv = 64;
float wrap = 39062;

void set_millis(int servoPin, float millis)
{
    pwm_set_gpio_level(servoPin, (millis/20000.f)*wrap);
}

void set_degrees(int servoPin, short deg) 
{
    if (deg > 180)
        deg = 180;

    if (deg < 0)
        deg = 0;

    float millis = MIN_MILLIS + ((deg / (float) (MAX_DEGREES - MIN_DEGREES)) * (MAX_MILLIS - MIN_MILLIS));
    set_millis(servoPin, millis);
}

void set_servo(int servoPin)
{
    gpio_set_function(servoPin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(servoPin);

    pwm_config config = pwm_get_default_config();
    
    uint64_t clockspeed = clock_get_hz((clock_index) 5);
    clockDiv = 64;
    wrap = 39062;

    while (clockspeed/clockDiv/50 > 65535 && clockDiv < 256) clockDiv += 64; 
    wrap = clockspeed/clockDiv/50;

    pwm_config_set_clkdiv(&config, clockDiv);
    pwm_config_set_wrap(&config, wrap);

    pwm_init(slice_num, &config, true);
}
