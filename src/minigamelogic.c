// game.c
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "neotrellis.h"
#include "minigame.h"

static int score = 0;
static uint8_t current_target = 255;   // 0–15 = valid, 255 = no target

// BPM control
static int bpm = 122;                  // change this for speed
static uint32_t beat_interval_ms = 0;
static uint32_t last_beat_ms = 0;

// Game duration (30 seconds)
static uint32_t game_duration_ms = 30000;  // 30 s
static uint32_t game_start_ms = 0;

// Forward declarations (only used inside this file)
static void advance_beat(void);
static TrellisCallback printKey(keyEvent evt);

static void advance_beat(void) {
    printf("Advancing beat...\n");

    clear_all_pixels();

    current_target = rand() % NEO_TRELLIS_NUM_KEYS;
    printf("New target key index: %u\n", current_target);

    // Hatsune Miku blue
    set_pixel_color(current_target, 0, 220, 255);
    show_pixels();

    printf("Target %u lit in Hatsune Miku blue.\n", current_target);
}

//when user presses a key
static TrellisCallback printKey(keyEvent evt) {

    if (evt.EDGE == SEESAW_KEYPAD_EDGE_RISING) {
        printf("Key pressed: %d (current_target = %d)\n", evt.NUM, current_target);

        if (evt.NUM == current_target) {
            score++;
            printf("Correct key! New score = %d\n", score);

            // Clear LEDs and wait for next beat
            printf("Clearing LEDs after correct hit.\n");
            clear_all_pixels();
            show_pixels();

            current_target = 255;   // no active target until next beat
            printf("current_target reset to 255.\n");
        } else {
            printf("Wrong key pressed.\n");
        }
    }

    return 0;
}

//Public: initialize game
void game_init(void) {
    printf("Initializing game logic...\n");

    // Setup keypad with our callback
    printf("init_keypad(printKey)\n");
    init_keypad(printKey);

    // Set up beat timing
    beat_interval_ms = 60000 / bpm;
    last_beat_ms = to_ms_since_boot(get_absolute_time());
    game_start_ms = last_beat_ms;

    printf("BPM = %d → beat_interval_ms = %u ms\n", bpm, beat_interval_ms);
    printf("Game duration = %u ms (~30 s)\n", game_duration_ms);

    // First target
    advance_beat();
}

//Public: one "step" of the game
void game_step(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // 1) Time's up?
    if (now - game_start_ms >= game_duration_ms) {
        printf("Time's up! Final score = %d\n", score);
        clear_all_pixels();
        show_pixels();

        // park here - or you could add restart logic, etc.
        while (1) {
            sleep_ms(1000);
        }
    }

    // Always process keypad events
    neo_read();

    // Check beat timing
    if (now - last_beat_ms >= beat_interval_ms) {
        printf("Beat timeout. now = %u, last_beat_ms = %u\n", now, last_beat_ms);
        last_beat_ms = now;
        advance_beat();
    }

    // tiny sleep so we don't busy-loop too hard
    sleep_ms(1);
}

// Optional getter
int game_get_score(void) {
    return score;
}
