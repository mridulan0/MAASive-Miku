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
#include "minigame.h"
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

int main() {
    stdio_init_all();

    // Seed RNG
    srand(200);
    
    // Initialize I2C
    init_i2c();
    sleep_ms(500);
    
    // Initialize NeoPixels
    if (init_neopixels() < 0) {
        printf("ERROR: Failed to initialize NeoPixels.\n");
        return 1;
    }

    // Initialize the game (sets up keypad callback, timers, first target)
    game_init();

    printf("Game initialized. Entering game loop...\n");

    while (true) {
        game_step();
        uint8_t combo = game_get_combo();
    }
    
    return 0;
}