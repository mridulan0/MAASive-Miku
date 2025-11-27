#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/structs/dma.h"
#include "hardware/structs/pwm.h"
#include "ievan_polkka_cut.h"

#define BUFFER_SIZE 1024
#define AUDIO_GPIO 27

void init_pwm_dma();
void fill_pwm_buffer();
void init_adc();
float get_multiplier();

extern uint16_t pwm_buffer[2][1024];
extern int current_buffer;
extern bool buffer_ready[2];
extern bool playback_active;
extern int dma_chan;

int main() {
    stdio_init_all();
    init_adc();
    init_pwm_dma();

    const uint8_t* wav_ptr = ievan_polkka_cut_wav + 44;
    int total_samples = (ievan_polkka_cut_wav_len - 44) / 2;
    int samples_played = 0;

    for (int i = 0; i < 2; i++) {
        int count = total_samples - samples_played;
        if (count > BUFFER_SIZE) {
            count = BUFFER_SIZE;
        }

        if (count <= 0) {
            break;
        }
        float multiplier = get_multiplier();
        fill_pwm_buffer(pwm_buffer[i], wav_ptr + samples_played * 2, count, multiplier);
        buffer_ready[i] = false;
        samples_played += count;
    }

    dma_channel_set_read_addr(dma_chan, pwm_buffer[0], true);
    dma_channel_start(dma_chan);

    while (samples_played < total_samples) {
        for (int i = 0; i < 2; i++) {
            if (buffer_ready[i]) {
                int count = total_samples - samples_played;
                if (count > BUFFER_SIZE) count = BUFFER_SIZE;
                if (count <= 0) {
                    playback_active = false;
                    continue;
                }
                
                float multiplier = get_multiplier();
                fill_pwm_buffer(pwm_buffer[i], wav_ptr + samples_played * 2, count, multiplier);
                buffer_ready[i] = false;
                samples_played += count;
            }
        }
        tight_loop_contents();
    }

    while (dma_channel_is_busy(dma_chan)) {
        tight_loop_contents();
    }

    dma_channel_set_irq0_enabled(dma_chan, false);
    irq_set_enabled(DMA_IRQ_0, false);
    dma_channel_abort(dma_chan);
    dma_channel_unclaim(dma_chan);

    uint slice = pwm_gpio_to_slice_num(AUDIO_GPIO);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(AUDIO_GPIO), 3905 / 2);

    printf("Playback finished.\n");
    return 0;
}