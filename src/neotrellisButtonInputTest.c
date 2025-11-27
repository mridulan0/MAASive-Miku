#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>            // <-- FIX: for bool

#include "pico/stdlib.h"
#include "hardware/i2c.h"

// NeoTrellis I2C address (default)
#define NEOTRELLIS_ADDR 0x2E

// I2C pins (adjust to your wiring)
#define I2C_SDA_PIN 28
#define I2C_SCL_PIN 29
#define I2C_PORT i2c0

// Seesaw registers
#define SEESAW_STATUS_BASE   0x00
#define SEESAW_GPIO_BASE     0x01
#define SEESAW_KEYPAD_BASE   0x10
#define SEESAW_NEOPIXEL_BASE 0x0E

// Seesaw keypad commands
#define SEESAW_KEYPAD_STATUS   0x00
#define SEESAW_KEYPAD_EVENT    0x01
#define SEESAW_KEYPAD_INTENSET 0x02
#define SEESAW_KEYPAD_INTENCLR 0x03
#define SEESAW_KEYPAD_COUNT    0x04
#define SEESAW_KEYPAD_FIFO     0x10   // <-- FIX: FIFO register for reading events

// Seesaw NeoPixel commands
#define SEESAW_NEOPIXEL_PIN        0x01
#define SEESAW_NEOPIXEL_SPEED      0x02
#define SEESAW_NEOPIXEL_BUF_LENGTH 0x03
#define SEESAW_NEOPIXEL_BUF        0x04
#define SEESAW_NEOPIXEL_SHOW       0x05

// NeoTrellis constants
#define NEO_TRELLIS_NUM_KEYS 16
#define NEO_TRELLIS_NUM_ROWS 4
#define NEO_TRELLIS_NUM_COLS 4

// Game constants
#define GAME_BPM 120
#define BEAT_INTERVAL_MS (60000 / GAME_BPM)

// Key event structure
typedef struct {
    uint8_t number;  // 0–15 logical button index (row-major)
    uint8_t edge;    // 0..3 (seesaw edge code)
} keyEvent;

// Write data to Seesaw register
int seesaw_write(uint8_t reg_base, uint8_t reg, uint8_t *data, uint8_t len) {
    uint8_t buf[34];
    buf[0] = reg_base;
    buf[1] = reg;

    for (int i = 0; i < len; i++) {
        buf[2 + i] = data[i];
    }

    int result = i2c_write_blocking(I2C_PORT, NEOTRELLIS_ADDR, buf, len + 2, false);
    sleep_ms(5);

    return result > 0 ? 0 : -1;
}

// Read data from Seesaw register
int seesaw_read(uint8_t reg_base, uint8_t reg, uint8_t *buf, uint8_t len) {
    uint8_t cmd[2] = {reg_base, reg};

    int result = i2c_write_blocking(I2C_PORT, NEOTRELLIS_ADDR, cmd, 2, true);
    if (result < 0) {
        return -1;
    }

    sleep_ms(1);

    result = i2c_read_blocking(I2C_PORT, NEOTRELLIS_ADDR, buf, len, false);
    if (result < 0) {
        return -1;
    }

    return 0;
}

// Initialize NeoPixels on the NeoTrellis
int init_neopixels() {
    uint8_t pin = 3;

    if (seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_PIN, &pin, 1) < 0) {
        printf("Failed to set NeoPixel pin\n");
        return -1;
    }

    uint8_t speed = 1;
    if (seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_SPEED, &speed, 1) < 0) {
        printf("Failed to set NeoPixel speed\n");
        return -1;
    }

    uint16_t buf_len = NEO_TRELLIS_NUM_KEYS * 3;
    uint8_t len_data[2] = {(buf_len >> 8) & 0xFF, buf_len & 0xFF};
    if (seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF_LENGTH, len_data, 2) < 0) {
        printf("Failed to set buffer length\n");
        return -1;
    }
    printf("Set NeoPixel buffer length to %d bytes\n", buf_len);

    return 0;
}

/* ----------------- KEYPAD SUPPORT ----------------- */

// Edge codes (match Adafruit/CircuitPython keypad driver)
#define KEYPAD_EDGE_HIGH    0
#define KEYPAD_EDGE_LOW     1
#define KEYPAD_EDGE_FALLING 2
#define KEYPAD_EDGE_RISING  3

// Configure an event for a given logical key (0–15)
int keypad_set_event(uint8_t logical_key, uint8_t edge, bool enable) {
    if (edge > 3) return -1;

    // Convert 0–15 matrix index to Seesaw "event key" numbering
    // Docs: keyIndex = (x / 4) * 8 + (x % 4)
    uint8_t event_key = (logical_key / NEO_TRELLIS_NUM_COLS) * 8
                      + (logical_key % NEO_TRELLIS_NUM_COLS);

    uint8_t cmd[2];
    cmd[0] = event_key;
    // From Adafruit driver: (1 << (edge+1)) | enable
    cmd[1] = (1 << (edge + 1)) | (enable ? 1 : 0);

    return seesaw_write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_EVENT, cmd, 2);
}

// Initialize keypad
int init_keypad() {
    // Enable keypad interrupts (mainly for INT pin, but harmless when polling)
    uint8_t inten = 1;
    if (seesaw_write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_INTENSET, &inten, 1) < 0) {
        printf("Failed to enable keypad interrupt\n");
        return -1;
    }

    // Enable rising and falling events on all 16 keys
    for (uint8_t k = 0; k < NEO_TRELLIS_NUM_KEYS; k++) {
        if (keypad_set_event(k, KEYPAD_EDGE_FALLING, true) < 0) {
            printf("Failed to set FALLING event for key %d\n", k);
            return -1;
        }
        if (keypad_set_event(k, KEYPAD_EDGE_RISING, true) < 0) {
            printf("Failed to set RISING event for key %d\n", k);
            return -1;
        }
    }

    printf("Keypad initialized\n");
    return 0;
}

// Read available keypad events
int read_keypad_events(keyEvent *events, int max_events) {
    uint8_t count_buf;

    // First, read how many events are available
    if (seesaw_read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &count_buf, 1) < 0) {
        return -1;
    }

    int num_events = count_buf;
    if (num_events == 0) {
        return 0;
    }

    if (num_events > max_events) {
        num_events = max_events;
    }

    // Read events from FIFO (NOT the EVENT register)  <-- FIX
    uint8_t fifo_buf[32];
    if (seesaw_read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_FIFO, fifo_buf, num_events) < 0) {
        return -1;
    }

    // Parse events
    for (int i = 0; i < num_events; i++) {
        uint8_t raw = fifo_buf[i];

        // Extract "seesaw key" and edge
        uint8_t seesaw_key = (raw >> 2) & 0x3F;
        uint8_t edge       = raw & 0x03;

        // Map seesaw key back to logical 0–15 matrix index
        // Docs: keyIndex = key / 8 * 4 + key % 8
        uint8_t logical = (seesaw_key / 8) * NEO_TRELLIS_NUM_COLS
                        + (seesaw_key % 8);

        events[i].number = logical;
        events[i].edge   = edge;
    }

    return num_events;
}

/* ----------------- NEOPIXEL HELPERS ----------------- */

// Set color of a specific key (pixel)
int set_pixel_color(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b) {
    if (pixel >= NEO_TRELLIS_NUM_KEYS) return -1;

    uint8_t buf[5];
    uint16_t offset = pixel * 3;

    buf[0] = (offset >> 8) & 0xFF;
    buf[1] = offset & 0xFF;
    buf[2] = g; // NeoPixels use GRB order
    buf[3] = r;
    buf[4] = b;

    int result = seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF, buf, 5);
    sleep_ms(2);
    return result;
}

// Show the pixels (update display)
int show_pixels() {
    uint8_t dummy = 0;
    return seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_SHOW, &dummy, 0);
}

// Clear all pixels
int clear_all_pixels() {
    for (int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++) {
        if (set_pixel_color(i, 0, 0, 0) < 0) {
            return -1;
        }
    }
    return show_pixels();
}

// Light up a column
void light_column(int col, uint8_t r, uint8_t g, uint8_t b) {
    for (int row = 0; row < NEO_TRELLIS_NUM_ROWS; row++) {
        int pixel = row * NEO_TRELLIS_NUM_COLS + col;
        set_pixel_color(pixel, r, g, b);
    }
    show_pixels();
}

// Check if button is in the active column
bool is_button_in_column(uint8_t button, int column) {
    int button_col = button % NEO_TRELLIS_NUM_COLS;
    return button_col == column;
}

/* ----------------- GAME LOGIC (UNCHANGED) ----------------- */

typedef struct {
    int active_column;
    int score;
    int misses;
    bool waiting_for_press;
} GameState;

// Play the game
void play_game() {
    GameState game = {0};
    game.active_column = -1;
    game.score = 0;
    game.misses = 0;
    game.waiting_for_press = false;

    uint32_t last_note_time = 0;

    printf("\n=== Piano Tiles Game Started! ===\n");
    printf("Press buttons in the lit column!\n");
    printf("BPM: %d (interval: %d ms)\n\n", GAME_BPM, BEAT_INTERVAL_MS);

    // Seed random number generator
    srand((unsigned)time_us_32());

    while (true) {
        uint32_t current_time = to_ms_since_boot(get_absolute_time());

        // Time to light up a new column
        if (current_time - last_note_time >= BEAT_INTERVAL_MS) {
            // Check if previous note was missed
            if (game.waiting_for_press) {
                game.misses++;
                printf("MISS! Score: %d | Misses: %d\n", game.score, game.misses);
            }

            // Clear previous column
            clear_all_pixels();

            // Select random column
            game.active_column = rand() % NEO_TRELLIS_NUM_COLS;

            // Light up the column (green)
            printf("New column: %d\n", game.active_column);
            light_column(game.active_column, 0, 50, 0);

            game.waiting_for_press = true;
            last_note_time = current_time;
        }

        // Check for button presses
        keyEvent events[16];
        int num_events = read_keypad_events(events, 16);

        if (num_events > 0) {
            for (int i = 0; i < num_events; i++) {
                // Only care about button "press" edges as you defined them
                if (events[i].edge == 1 || events[i].edge == 3) {
                    printf("Button pressed: %d (edge: %d)\n",
                           events[i].number, events[i].edge);

                    if (game.waiting_for_press) {
                        // Check if correct button
                        if (is_button_in_column(events[i].number, game.active_column)) {
                            game.score++;
                            printf("HIT! Score: %d | Misses: %d\n",
                                   game.score, game.misses);

                            // Flash the column white for feedback
                            light_column(game.active_column, 50, 50, 50);
                            sleep_ms(100);
                            light_column(game.active_column, 0, 50, 0);

                            game.waiting_for_press = false;
                        } else {
                            // Wrong button pressed
                            game.misses++;
                            printf("WRONG BUTTON! Score: %d | Misses: %d\n",
                                   game.score, game.misses);

                            // Flash red for feedback
                            int wrong_col = events[i].number % NEO_TRELLIS_NUM_COLS;
                            light_column(wrong_col, 50, 0, 0);
                            sleep_ms(100);
                            light_column(game.active_column, 0, 50, 0);
                        }
                    }
                }
            }
        }

        // Small delay to prevent CPU hogging
        sleep_ms(10);
    }
}

int main() {
    stdio_init_all();

    // Initialize I2C
    i2c_init(I2C_PORT, 100 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    printf("NeoTrellis Piano Tiles Game\n");
    sleep_ms(500);

    // Initialize NeoPixels
    printf("Initializing NeoPixels...\n");
    if (init_neopixels() < 0) {
        printf("Failed to initialize NeoPixels\n");
        return 1;
    }

    // Initialize keypad
    printf("Initializing Keypad...\n");
    if (init_keypad() < 0) {
        printf("Failed to initialize keypad\n");
        return 1;
    }

    printf("Initialization complete!\n");

    // Clear display
    clear_all_pixels();
    sleep_ms(500);

    // Test: light up all buttons briefly to confirm LEDs work
    printf("Testing LEDs...\n");
    for (int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++) {
        set_pixel_color(i, 10, 10, 10);
    }
    show_pixels();
    sleep_ms(1000);
    clear_all_pixels();
    sleep_ms(500);

    // Start the game
    play_game();

    return 0;
}
