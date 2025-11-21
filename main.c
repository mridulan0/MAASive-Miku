#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/dma.h"

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
    //initialize all
    stdio_init_all();

    //initialize adc
    float pwm_freq = 0;
    int adc_channel = 5;
    init_adc(adc_channel);
    init_adc_for_freerun();

    //initialize pwm
    int pwm_channel = 0;

    //freerunning output
    for(;;) {
        pwm_freq = (adc_hw->result) / 7.0;
        modify_frequency(pwm_channel, pwm_freq);
        fflush(stdout);
        sleep_ms(1000);
    }
}