#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/structs/dma.h"
#include "hardware/structs/pwm.h"
#include <stdint.h>
#include <stdbool.h>

#define AUDIO_GPIO 28
#define PWM_TOP 3905
#define BUFFER_SIZE 1024

uint16_t pwm_buffer[2][BUFFER_SIZE];
int current_buffer = 0;
bool buffer_ready[2] = {false, false};
bool playback_active = true;

int dma_chan;

void dma_handler() {
    dma_hw->ints0 = 1u << dma_chan;

    if (playback_active) {
        buffer_ready[current_buffer] = true;
        current_buffer ^= 1;
        dma_channel_set_read_addr(dma_chan, pwm_buffer[current_buffer], true);
    }
}

void init_pwm_dma() {
    gpio_set_function(AUDIO_GPIO, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(AUDIO_GPIO);
    pwm_set_wrap(slice, PWM_TOP);
    pwm_set_clkdiv(slice, 1.22f);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(AUDIO_GPIO), PWM_TOP / 2);
    pwm_set_enabled(slice, true);

    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, DREQ_PWM_WRAP0 + slice);

    dma_channel_configure(
        dma_chan,
        &c,
        &pwm_hw->slice[slice].cc,
        NULL,
        BUFFER_SIZE,
        false
    );

    dma_channel_set_irq0_enabled(dma_chan, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

void fill_pwm_buffer(uint16_t* dest, const uint8_t* src, int count) {
    for (int i = 0; i < count; i++) {
        int16_t sample = src[0] | (src[1] << 8);
        src += 2;
        dest[i] = ((int32_t)sample + 32768) * PWM_TOP / 65535;
    }
}