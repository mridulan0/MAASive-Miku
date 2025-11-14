#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
#include <math.h> 
#include <stdlib.h>
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
/****************************************** */
// Uncomment the following #define when 
// you are ready to run Step 3.

// WARNING: The process will take a VERY 
// long time as it compiles and uploads 
// all the image frames into the uploaded 
// binary!  Expect to wait 5 minutes.
#define ANIMATION

/****************************************** */
#ifdef ANIMATION
#include "images.h"
#endif
/****************************************** */
int determine_gpio_from_channel(int);
void init_adc(int);
void init_adc_for_freerun();
void modify_frequency(int, float);

void init_spi_lcd();
Picture* load_image(const char* image_data);
void free_image(Picture* pic);

int main() {
  //initialize all
    stdio_init_all();

    // //initialize adc
    // float pwm_freq = 0;
    // int adc_channel = 5;
    // init_adc(adc_channel);
    // init_adc_for_freerun();

    // //initialize pwm
    // int pwm_channel = 0;

    // //freerunning output
    // for(;;) {
    //     pwm_freq = (adc_hw->result) / 7.0;
    //     modify_frequency(pwm_channel, pwm_freq);
    //     fflush(stdout);
    //     sleep_ms(1000);
    // }

    // initialize lcd screen
    init_spi_lcd();
    LCD_Setup();
    LCD_Clear(0xC71D); // Clear the screen to black

    #ifdef ANIMATION
    Picture* frame_pic = NULL;
    int frame_index = 0;
    int combo = 1;
    bool valid_hit = false;
    bool miss = false;
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
        // frame_index++;
        if (frame_index >= mystery_frame_count) {
            frame_index = 0;
        }
        
        // Add combo count to the screen
        if(valid_hit){
            combo++;
            valid_hit = false;
        }
        if(miss){
            combo = 0;
            miss = false;
        }
        if (combo > 0){
            char str[5] = "Combo";
            LCD_DrawString(20, 200, BLACK, 0x00, str, 32, true);
            char com_str[3] ={};
            sprintf(com_str, "%d", combo);
            LCD_DrawString(150, 200, BLACK, 0x00, com_str, 32, true);
        }
        // Add a small delay to control animation speed
        sleep_us(500); // Adjust delay as needed
    }
    #endif

    for(;;);
}