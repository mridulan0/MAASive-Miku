#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// NeoTrellis I2C address (default)
#define NEOTRELLIS_ADDR 0x2E

// I2C pins (adjust to your wiring)
#define I2C_SDA_PIN 28
#define I2C_SCL_PIN 29
#define I2C_PORT i2c0

// Seesaw registers
#define SEESAW_STATUS_BASE 0x00
#define SEESAW_GPIO_BASE 0x01
#define SEESAW_KEYPAD_BASE 0x10
#define SEESAW_NEOPIXEL_BASE 0x0E

// Seesaw GPIO/Keypad commands
#define SEESAW_GPIO_BULK 0x04
#define SEESAW_GPIO_BULK_SET 0x05
#define SEESAW_KEYPAD_EVENT 0x01
#define SEESAW_KEYPAD_INTENSET 0x02
#define SEESAW_KEYPAD_INTENCLR 0x03

// Seesaw NeoPixel commands
#define SEESAW_NEOPIXEL_PIN 0x01
#define SEESAW_NEOPIXEL_SPEED 0x02
#define SEESAW_NEOPIXEL_BUF_LENGTH 0x03
#define SEESAW_NEOPIXEL_BUF 0x04
#define SEESAW_NEOPIXEL_SHOW 0x05

// NeoTrellis constants
#define NEO_TRELLIS_NUM_KEYS 16
#define NEO_TRELLIS_NUM_ROWS 4
#define NEO_TRELLIS_NUM_COLS 4

// Game constants
#define GAME_BPM 120  // Beats per minute - adjust this to change speed
#define BEAT_INTERVAL_MS (60000 / GAME_BPM)

// Key event structure
typedef struct {
    uint8_t number;
    uint8_t edge;  // 1 = rising (press), 2 = falling (release)
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
    
    if (i2c_write_blocking(I2C_PORT, NEOTRELLIS_ADDR, cmd, 2, true) < 0) {
        return -1;
    }
    
    sleep_ms(5);
    
    if (i2c_read_blocking(I2C_PORT, NEOTRELLIS_ADDR, buf, len, false) < 0) {
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
    
    return 0;
}

// Initialize keypad
int init_keypad() {
    // Enable all key events
    uint8_t data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    if (seesaw_write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_INTENSET, data, 4) < 0) {
        printf("Failed to enable keypad interrupts\n");
        return -1;
    }
    
    printf("Keypad initialized\n");
    return 0;
}

// Read keypad events
int read_keypad(keyEvent *event) {
    uint8_t buf[2];
    
    if (seesaw_read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_EVENT, buf, 2) < 0) {
        return -1;
    }
    
    // Check if there's a valid event (both bytes should not be 0)
    if (buf[0] == 0 && buf[1] == 0) {
        return 0; // No event
    }
    
    event->number = (buf[0] >> 2) & 0x3F;
    event->edge = buf[0] & 0x03;
    
    return 1; // Event available
}

// Set color of a specific key (pixel)
int set_pixel_color(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b) {
    if (pixel >= NEO_TRELLIS_NUM_KEYS) return -1;
    
    uint8_t buf[5];
    uint16_t offset = pixel * 3;
    
    buf[0] = (offset >> 8) & 0xFF;
    buf[1] = offset & 0xFF;
    buf[2] = g;
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

// Game state
typedef struct {
    int active_column;  // Which column is currently lit (0-3)
    int score;
    int misses;
    bool waiting_for_press;
} GameState;

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
    printf("BPM: %d\n\n", GAME_BPM);
    
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
            light_column(game.active_column, 0, 50, 0);
            
            game.waiting_for_press = true;
            last_note_time = current_time;
        }
        
        // Check for button presses
        keyEvent event;
        if (read_keypad(&event) > 0) {
            // Only care about button presses (rising edge)
            if (event.edge == 1) {
                printf("Button pressed: %d\n", event.number);
                
                if (game.waiting_for_press) {
                    // Check if correct button
                    if (is_button_in_column(event.number, game.active_column)) {
                        game.score++;
                        printf("HIT! Score: %d | Misses: %d\n", game.score, game.misses);
                        
                        // Flash the column white for feedback
                        light_column(game.active_column, 50, 50, 50);
                        sleep_ms(100);
                        light_column(game.active_column, 0, 50, 0);
                        
                        game.waiting_for_press = false;
                    } else {
                        // Wrong button pressed
                        game.misses++;
                        printf("WRONG BUTTON! Score: %d | Misses: %d\n", game.score, game.misses);
                        
                        // Flash red for feedback
                        int wrong_col = event.number % NEO_TRELLIS_NUM_COLS;
                        light_column(wrong_col, 50, 0, 0);
                        sleep_ms(100);
                        light_column(game.active_column, 0, 50, 0);
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
    
    // Start the game
    play_game();
    
    return 0;
}