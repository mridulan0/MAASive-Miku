// game.c
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "neotrellis.h"
#include "minigame.h"

static int score = 0;
static bool chg = false;
static uint8_t current_target = 255;   // 0–15 = valid, 255 = no target
static uint8_t prev_target = 255;

// Condition control variables for gameplay reaction time
static bool cur_check = false;
static bool nxt_check = false;
static uint8_t keyHit = 0;

// BPM control
static int bpm = 120;                  // change this for speed
static uint32_t beat_interval_ms = 0;
static uint32_t last_beat_ms = 0;
static uint32_t nxt_beat_ms = 0;

// Game duration (30 seconds)
static uint32_t game_duration_ms = 33000;  // 33 s
static uint32_t game_start_ms = 0;

static uint8_t beats[70] = {0}; 
static uint8_t cur_idx = 0;

bool valid_hit = false;
bool miss = false;

static void advance_beat(bool clear, bool update, uint8_t* target) {
    printf("Advancing beat...\n");

    if(((*target) != 255 || valid_hit) && clear){
         if (!valid_hit) {
            miss = true;
            chg = true;
            if(prev_target!= current_target)
                set_pixel_color(*target, 0, 0 ,0);
            *target = 255;
         }
         else{
            valid_hit = false;
            if(prev_target!= current_target)
                set_pixel_color(keyHit, 0, 0 ,0);
            chg = false;
         }
    }

    if(update){
        chg = false;
        *target = beats[cur_idx++];
        printf("New target key index: %u\n", *target);
        // Hatsune Miku blue
        set_pixel_color(*target, 0, 220, 255);

        printf("Target %u lit in Hatsune Miku blue.\n", *target);
    }

    show_pixels();
}

//when user presses a key
static TrellisCallback printKey(keyEvent evt) {

    if (evt.EDGE == SEESAW_KEYPAD_EDGE_RISING) {

        if (evt.NUM == current_target || evt.NUM == prev_target) {
            score++;
            chg = true;
            printf("Correct key! New score = %d\n", score);

            // Clear LEDs and wait for next beat
            if (current_target!= prev_target)
                set_pixel_color(evt.NUM == prev_target?  prev_target: current_target, 152, 251, 152);
            valid_hit = true;
            show_pixels();

            if (evt.NUM == prev_target){
                keyHit = prev_target;
                prev_target = 255;
            }
            else{
                keyHit = current_target;
                current_target = 255; 
            }
        } else {
            printf("Wrong key pressed.\n");
        }
    }

    return 0;
}

// Initialize game
void game_init(void) {
    printf("Initializing game logic...\n");
    init_keypad(printKey);

    srand(250);
    // Set up beat timing
    beat_interval_ms = 60000 / bpm;
    last_beat_ms = to_ms_since_boot(get_absolute_time());
    game_start_ms = last_beat_ms;
    clear_all_pixels();

    // Avoid overlapping keys (really hard to catch when playing)
    for(int i = 0; i<70; i++){
        beats[i] = rand() % NEO_TRELLIS_NUM_KEYS;
        if(i>0 && beats[i] == beats[i-1])
            beats[i] = (beats[i] + 1) % NEO_TRELLIS_NUM_KEYS;
    }

    printf("BPM = %d → beat_interval_ms = %u ms\n", bpm, beat_interval_ms);
    printf("Game duration = %u ms (~33 s)\n", game_duration_ms);
}

//One "step" of the game
void game_step(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (miss){
        score = 0;
        miss = false;
    }

    // 1) Time's up?
    if (now - game_start_ms >= game_duration_ms) {
        printf("Time's up! Final score = %d\n", score);
        clear_all_pixels();

        // park here - or you could add restart logic, etc.
        while (1) {
            sleep_ms(1000);
        }
    }

    // Always process keypad events
    neo_read();

    // Pad timing by +- 100ms for reaction time
    if (now - last_beat_ms + 100 >= beat_interval_ms && !cur_check) {
        cur_check = true;
        nxt_check = false;
        prev_target = current_target;

        //only update the beat, don't clear the pixel
        advance_beat(false, true, &current_target);
    }

    if (now - last_beat_ms >= beat_interval_ms && !nxt_check){
        //update next beat time
        nxt_beat_ms = now;
        nxt_check = true;

        //warning color
        set_pixel_color(prev_target, 100, 0, 0);
        show_pixels();
    }

    if (now - last_beat_ms - 100 >= beat_interval_ms){
        cur_check = false;

        //update reference interval time, clear previous pixel
        advance_beat(true, false, &prev_target);
        last_beat_ms = nxt_beat_ms;
    }

    // tiny sleep so we don't busy-loop too hard
    sleep_ms(1);
}

// Optional getter
int game_get_combo(void) {
    return score;
}
bool get_chg(void){
    return chg;
}