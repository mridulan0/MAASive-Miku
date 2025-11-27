#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "neotrellis.h"
#include "pico/time.h"
//////////////////////////////////////////////////////////////////////////////

// const char* username1 = "sewell7";
// const char* username2 = "";
// const char* username3 = "";
// const char* username4 = "";

//////////////////////////////////////////////////////////////////////////////

// Function Declarations
int determine_gpio_from_channel(int);
void init_adc(int);
void init_adc_for_freerun();
void modify_frequency(int, float);
void advance_beat(void);
void fade_pixel_miku(uint8_t pixel, uint32_t duration_ms);

//////////////////////////////////////////////////////////////////////////////

// Global Variable Declarations
// int step0 = 0;
// int offset0 = 0;
// int step1 = 0;
// int offset1 = 0;

//////////////////////////////////////////////////////////////////////////////

// mini game variables
int score = 0;// player's score
uint8_t current_target = 255;// which key should be hit (0–15), 255 = no target

// BPM control
int bpm = 120;// change this for speed
uint32_t beat_interval_ms = 0;// ms between beats
uint32_t last_beat_ms = 0; // last time we advanced
// Game duration (30 seconds)
uint32_t game_duration_ms = 34000;  // 30,000 ms = 30 s
uint32_t game_start_ms = 0;

//game logic
void advance_beat(void) {
    // turn everything off first
    clear_all_pixels();

    // choose a new random key
    current_target = rand() % NEO_TRELLIS_NUM_KEYS;

    // light that key (your set_pixel_color produces random colors)
    //fading version
    fade_pixel_miku(current_target, 1000);

    printf("New target: %d\n", current_target);
}

//when user presses key
TrellisCallback printKey(keyEvent evt) {

    if (evt.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        printf("Pressed %d (target %d)\n", evt.NUM, current_target);

        if (evt.NUM == current_target) {
            score++;
            printf("Correct! Score = %d\n", score);

            // Clear LEDs and wait for next beat to choose new target
            clear_all_pixels();
            show_pixels();

            current_target = 255;   // disable hits until next beat
        } else {
            printf("Wrong key!\n");
        }
    }

    return 0;
}

int main() {
    // Initialize stdio for printf
    stdio_init_all();
    srand(time_us_32());
    
    // Initialize I2C
    printf("Initializing I2C...\n");
    init_i2c();
    sleep_ms(500);
    printf("I2C initialized.\n");
    
    // Initialize NeoPixels
    printf("Initializing NeoPixels (Seesaw)...\n");
    if (init_neopixels() < 0) {
        printf("Failed to initialize NeoPixels\n");
        return 1;
    }
    printf("NeoPixels initialized successfully!\n");
    init_keypad(printKey);

    //beat timing
    beat_interval_ms = 60000 / bpm;  // interval in ms for given BPM
    last_beat_ms = to_ms_since_boot(get_absolute_time());
    printf("BPM set to %d → beat_interval_ms = %u ms\n", bpm, beat_interval_ms);

    printf("Starting first beat...\n");
    advance_beat();
    

    while (true) {
        // 2) Beat timing check
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - game_start_ms >= game_duration_ms) {
            printf("Time's up! Final score = %d\n", score);
            clear_all_pixels();
            show_pixels();

            while (1) {
                sleep_ms(1000);
            }
        }
        neo_read();

        if (now - last_beat_ms >= beat_interval_ms) {
            printf("Beat timeout reached. now = %u, last_beat_ms = %u\n", now, last_beat_ms);
            last_beat_ms = now;
            advance_beat();
        }
        //maybe can edit or remove depends on the play...
        sleep_ms(1);
    }
    
    return 0;
}