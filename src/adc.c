#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"

//////////////////////////////////////////////////////////////////////////////

// const char* username = "sewell7";

//////////////////////////////////////////////////////////////////////////////
/*

objective: create an adc that can automatically convert between the analog
input signal from the potentiometer to a digital signal that can be used
by the PWM.

ideally, this will be used to change the amplitude and frequency of the 
song.

notes: after doing some research and talking to ta's, I learned that the
adc might not be the best tool to modify the volume of the audio output by
the pwm. Instead, I will build both an adc and a dac. The adc will be used
to modify the frequency of the input to the pwm, and the dac will be used
to modify the volume of the pwm's output.

*/
//////////////////////////////////////////////////////////////////////////////

// Global Variable Declarations
int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;

//////////////////////////////////////////////////////////////////////////////

// Function Definitions

//////////////////////////////////////////////////////////////////////////////

int determine_gpio_from_channel (int channel) {
    int gpio_pin;

    gpio_pin = 40 + channel;

    return gpio_pin;
}

void init_adc(int channel) {
    //initialize adc
    adc_init();

    //initialize adc gpio
    int gpio_pin = determine_gpio_from_channel(channel);
    gpio_set_function(gpio_pin, GPIO_FUNC_NULL);
    gpio_disable_pulls(gpio_pin);
    gpio_set_input_enabled(gpio_pin, false);

    //set the adc
    hw_write_masked(&adc_hw->cs, channel << 12u, 0x0000f000);
}

void init_adc_for_freerun () {

    hw_set_bits(&adc_hw->cs, ADC_CS_EN_BITS);
    hw_set_bits(&adc_hw->cs, ADC_CS_START_MANY_BITS);

    return;
}

void modify_frequency(int channel, float freq) {

    printf("frequency: %f, ", freq);

    if (channel == 0 && freq == 0.0) {
        step0 = 0;
        offset0 = 0;
    }
    else if (channel == 1 && freq == 0.0) {
        step1 = 0;
        offset1 = 0;
    }
    else if (channel == 0 && freq != 0.0) {
        step0 = ((freq * 1000) / 20000) * (1<<16);
    }
    else if (channel == 1 && freq != 0.0) {
        step1 = ((freq * 1000) / 20000) * (1<<16);
    }

    printf("pwm input: %d\n", step0);

    return;
}

//////////////////////////////////////////////////////////////////////////////

