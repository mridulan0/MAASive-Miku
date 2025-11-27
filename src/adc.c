#include "pico/stdlib.h"
#include "hardware/adc.h"

void init_adc() {
    adc_init();
    adc_gpio_init(45);
    adc_select_input(5);
}

float get_multiplier() {
    uint16_t adc_val = adc_read();
    float multiplier = adc_val / 4095.0f;

    return multiplier;
}