#include "pico/stdlib.h"
#include "hardware/pwm.h"
// #include "pico/ff_stdio.h"

#define GPIO 28

#define C 262
#define Csharp 277
#define D 294
#define E 330
#define F 349
#define Fsharp 370
#define G 392
#define Gsharp 415
#define A 440
#define B 494
#define C2 523
#define C2sharp 554

//////////////////////////////////////////////////////////////////////////////

void play_tone(uint gpio, int freq, int duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    uint channel = pwm_gpio_to_channel(gpio);

    // calculate clkdiv to keep wrap <= 65535 = 2^16
    float clkdiv = (float)125000000 / (freq * 65535.0);
    if (clkdiv < 1.0f) clkdiv = 1.0f; // 1 is the minimum allowed

    pwm_set_clkdiv(slice_num, clkdiv);
    pwm_set_wrap(slice_num, 65535);
    pwm_set_chan_level(slice_num, channel, 32768); // 50% duty

    pwm_set_enabled(slice_num, true);
    sleep_ms(duration_ms); // so note plays intended length
    pwm_set_chan_level(slice_num, channel, 0);
}

//////////////////////////////////////////////////////////////////////////////

int main() {
    stdio_init_all();
    gpio_set_function(GPIO, GPIO_FUNC_PWM);

    // ievan polkka
    struct {
        int freq;
        int duration;
    } song[] = {
        {Csharp, 500}, {Fsharp, 500}, {Fsharp, 750}, {Gsharp, 250}, 
        {A, 250}, {A, 250}, {Fsharp, 250}, {Fsharp, 250},
        {Fsharp, 500}, {Fsharp, 250}, {A, 250}, {Gsharp, 500},
        {E, 500}, {E, 500}, {Gsharp, 500}, {A, 500},
        {Fsharp, 500}, {Fsharp, 500}, {Fsharp, 250}, {Fsharp, 250},
        {Csharp, 500}, {Fsharp, 500}, {Fsharp, 750}, {Gsharp, 250},
        {A, 500}, {Fsharp, 500}, {Fsharp, 500}, {Fsharp, 250},
        {A, 250}, {C2sharp, 250}, {C2sharp, 250}, {C2sharp, 250},
        {B, 250}, {A, 500}, {Gsharp, 500}, {A, 500},
        {Fsharp, 500}, {Fsharp, 500}, {Fsharp, 250}, {A, 250},
        {C2sharp, 500}, {C2sharp, 500}, {B, 500}, {A, 500},
        {Gsharp, 500}, {E, 500}, {E, 500}, {E, 250},
        {Gsharp, 250}, {B, 250}, {B, 250}, {B, 250},
        {B, 250}, {A, 250}, {A, 250}, {Gsharp, 250},
        {Gsharp, 250}, {A, 500}, {Fsharp, 500}, {Fsharp, 500},
        {Fsharp, 250}, {A, 250}, {C2sharp, 500}, {C2sharp, 500}, 
        {B, 500}, {A, 500}, {Gsharp, 500}, {E, 500}, 
        {E, 500}, {E, 250}, {Gsharp, 250}, {B, 250}, 
        {B, 250}, {B, 250}, {B, 250}, {A, 250}, 
        {A, 250}, {Gsharp, 250}, {Gsharp, 250}, {A, 500}, 
        {Fsharp, 500}, {Fsharp, 750}, {Fsharp, 250}
    };

    int song_length = sizeof(song)/sizeof(song[0]);

    for(;;) {
        for (int i = 0; i < song_length; i++) {
            play_tone(GPIO, song[i].freq, song[i].duration);
            sleep_ms(50);
        }
    }

    return 0;
}