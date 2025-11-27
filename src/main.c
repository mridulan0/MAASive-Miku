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

//////////////////////////////////////////////////////////////////////////////

// Global Variable Declarations
// int step0 = 0;
// int offset0 = 0;
// int step1 = 0;
// int offset1 = 0;

//////////////////////////////////////////////////////////////////////////////

int main() {
    // Initialize stdio for printf
    stdio_init_all();
    
    // Initialize I2C
    init_i2c();
    sleep_ms(500);
    
    // Initialize NeoPixels
    if (init_neopixels() < 0) {
        printf("Failed to initialize NeoPixels\n");
        return 1;
    }
    init_keypad(printKey);
    printf("NeoPixels initialized successfully!\n");

    while (true) {
        neo_read();
    }
    
    return 0;
}