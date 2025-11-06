#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define GPIO 28

#define C4 262
#define D4 294
#define E4 330
#define F4 349
#define G4 392
#define A4 440
#define B4 494
#define C5 523

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

    // Twinkle Twinkle Little Star
    struct {
        int freq;
        int duration;
    } song[] = {
        {C4, 500}, {C4, 500}, {G4, 500}, {G4, 500},
        {A4, 500}, {A4, 500}, {G4, 1000},
        {F4, 500}, {F4, 500}, {E4, 500}, {E4, 500},
        {D4, 500}, {D4, 500}, {C4, 1000}
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