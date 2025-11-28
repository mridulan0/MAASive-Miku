#include "pico/stdlib.h"
#include "pico/multicore.h"

#include <stdio.h>
#include <string.h>
#include <math.h> 
#include <stdlib.h>

#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/i2c.h"
#include "neotrellis.h"
#include "minigame.h"
#include "pico/time.h"
//////////////////////////////////////////////////////////////////////////////
#include "hardware/adc.h"
#include "hardware/structs/dma.h"
#include "hardware/structs/pwm.h"

#include "lcd.h"
#include "images.h"
#include "combo.h"
#include "ievan_polkka_cut.h"

#define BUFFER_SIZE 1024
#define AUDIO_GPIO 27
bool input = false;

/****************************************** */
// Comment out this line once the keypad is implemented
#define PLAY
/****************************************** */
#ifdef PLAY
static void _play_handler(){
    if (gpio_get_irq_event_mask(21) == GPIO_IRQ_EDGE_RISE) {
            input = true;
            gpio_acknowledge_irq(21, GPIO_IRQ_EDGE_RISE);
        }
}
static void _init_play(){
    gpio_init(21);
    gpio_add_raw_irq_handler(21, _play_handler);
    irq_set_enabled(21, true);
    gpio_set_irq_enabled(21, GPIO_IRQ_EDGE_RISE, true);
    
}
#endif

// lcd display functions
void init_spi_lcd();
Picture* load_image(const uint8_t* image_data);
void free_image(Picture* pic);

// Function Declarations
int determine_gpio_from_channel(int);
void init_adc(int);
void init_adc_for_freerun();
void modify_frequency(int, float);
void advance_beat(void);
void fade_pixel_miku(uint8_t pixel, uint32_t duration_ms);
// song playing functions
void init_pwm_dma();
void fill_pwm_buffer();

// volume control functions
void init_adc();
float get_multiplier();

extern uint16_t pwm_buffer[2][1024];
extern int current_buffer;
extern bool buffer_ready[2];
extern bool playback_active;
extern int dma_chan;

void core1_main() {
    // initialize lcd screen
    init_spi_lcd();
    LCD_Setup();
    LCD_Clear(0xC71D); // Clear the screen to black

    Picture* frame_pic = NULL;
    int frame_index = 0;
    int combo = 0;
    bool valid_hit = false;
    bool miss = false;
    bool chg = false;
    int ten = combo/10;
    int one = combo%10;

    #ifdef PLAY
        _init_play();
    #endif

    while (1) { // Loop forever
        // Get the next frame from the array
        frame_pic = load_image(mystery_frames[frame_index]);
    
        if (frame_pic) {
            // Draw the frame to the top-left corner of the screen
            LCD_DrawPicture(0, 0, frame_pic);
            
            // Free the Picture struct (not the pixel data)
            free_image(frame_pic);
        }
    
        // Move to the next frame, looping back to the start
        frame_index++;
        if (frame_index >= mystery_frame_count) {
            frame_index = 0;
        }

        if (input){
            valid_hit = true;
            input=false;
        }
        // Add combo count to the screen
        if(valid_hit){
            combo++;
            chg = true;
            valid_hit = false;
        }
        if(miss){
            combo = 0;
            miss = false;
        }
        if(chg){
            _disp_combo_help(combo, &ten, &one);
            chg = false;
        }
        // Add a small delay to control animation speed
        sleep_us(40); // Adjust delay as needed
    }
}

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
    multicore_launch_core1(core1_main);

    // initialize pwm and dma
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

    // plays the song
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

    for(;;);
}