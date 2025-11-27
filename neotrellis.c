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
#define SEESAW_NEOPIXEL_BASE 0x0E

// Seesaw NeoPixel commands
#define SEESAW_NEOPIXEL_PIN 0x01
#define SEESAW_NEOPIXEL_SPEED 0x02
#define SEESAW_NEOPIXEL_BUF_LENGTH 0x03
#define SEESAW_NEOPIXEL_BUF 0x04
#define SEESAW_NEOPIXEL_SHOW 0x05

// NeoTrellis constants
#define NEO_TRELLIS_NUM_KEYS 16

// Write data to Seesaw register
int seesaw_write(uint8_t reg_base, uint8_t reg, uint8_t *data, uint8_t len) {
    uint8_t buf[34];
    buf[0] = reg_base;
    buf[1] = reg;
    
    for (int i = 0; i < len; i++) {
        buf[2 + i] = data[i];
    }
    
    int result = i2c_write_blocking(I2C_PORT, NEOTRELLIS_ADDR, buf, len + 2, false);
    sleep_ms(5); // Increased delay for more reliable communication
    
    return result > 0 ? 0 : -1;
}

// Initialize NeoPixels on the NeoTrellis
int init_neopixels() {
    uint8_t pin = 3; // NeoPixel pin on Seesaw
    
    // Set NeoPixel pin
    if (seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_PIN, &pin, 1) < 0) {
        printf("Failed to set NeoPixel pin\n");
        return -1;
    }
    
    // Set NeoPixel speed (800 KHz)
    uint8_t speed = 1;
    if (seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_SPEED, &speed, 1) < 0) {
        printf("Failed to set NeoPixel speed\n");
        return -1;
    }
    
    // Set buffer length (16 pixels * 3 bytes per pixel = 48 bytes)
    uint16_t buf_len = NEO_TRELLIS_NUM_KEYS * 3;  // Total bytes, not pixel count
    uint8_t len_data[2] = {(buf_len >> 8) & 0xFF, buf_len & 0xFF};
    if (seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIXEL_BUF_LENGTH, len_data, 2) < 0) {
        printf("Failed to set buffer length\n");
        return -1;
    }
    printf("Set NeoPixel buffer length to %d bytes\n", buf_len);
    
    return 0;
}

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
    sleep_ms(2); // Add small delay between pixel writes
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

// Light up all buttons with same color
int light_all_buttons(uint8_t r, uint8_t g, uint8_t b) {
    // Write all pixels to buffer first
    for (int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++) {
        if (set_pixel_color(i, r, g, b) < 0) {
            printf("Failed to set pixel %d\n", i);
            return -1;
        }
    }
    // Then show all at once
    sleep_ms(10); // Small delay before show command
    return show_pixels();
}

// Create a rainbow pattern
int rainbow_pattern() {
    uint8_t colors[16][3] = {
        {255, 0, 0}, {255, 32, 0}, {255, 64, 0}, {255, 128, 0},
        {255, 255, 0}, {128, 255, 0}, {0, 255, 0}, {0, 255, 128},
        {0, 255, 255}, {0, 128, 255}, {0, 0, 255}, {64, 0, 255},
        {128, 0, 255}, {255, 0, 255}, {255, 0, 128}, {255, 0, 64}
    };
    
    for (int i = 0; i < 16; i++) {
        if (set_pixel_color(i, colors[i][0], colors[i][1], colors[i][2]) < 0) {
            return -1;
        }
    }
    return show_pixels();
}

// Animate a moving light
void chase_animation() {
    for (int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++) {
        clear_all_pixels();
        set_pixel_color(i, 0, 50, 100);
        show_pixels();
        sleep_ms(100);
    }
}

int main() {
    // Initialize stdio for printf
    stdio_init_all();
    
    // Initialize I2C
    i2c_init(I2C_PORT, 100 * 1000); // 100 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    
    printf("NeoTrellis Demo Starting...\n");
    sleep_ms(500);
    
    // Initialize NeoPixels
    printf("Initializing NeoPixels...\n");
    if (init_neopixels() < 0) {
        printf("Failed to initialize NeoPixels\n");
        return 1;
    }
    
    printf("NeoPixels initialized successfully!\n");
    
    // Main loop with different patterns
    while (true) {
        // Clear all
        printf("Clearing...\n");
        clear_all_pixels();
        sleep_ms(1000);
        
        // Red
        printf("Red...\n");
        light_all_buttons(50, 0, 0);
        sleep_ms(2000);
        
        // Green
        printf("Green...\n");
        light_all_buttons(0, 50, 0);
        sleep_ms(2000);
        
        // Blue
        printf("Blue...\n");
        light_all_buttons(0, 0, 50);
        sleep_ms(2000);
        
        // Rainbow
        printf("Rainbow...\n");
        rainbow_pattern();
        sleep_ms(3000);
        
        // Chase animation
        printf("Chase...\n");
        for (int i = 0; i < 3; i++) {
            chase_animation();
        }
    }
    
    return 0;
}