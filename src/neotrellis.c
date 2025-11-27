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

// Keypad commands
#define SEESAW_KEYPAD_BASE 0x10
#define SEESAW_KEYPAD_STATUS 0x00
#define SEESAW_KEYPAD_EVENT 0x01
#define SEESAW_KEYPAD_INTENSET 0x02
#define SEESAW_KEYPAD_INTENCLR 0x03
#define SEESAW_KEYPAD_COUNT 0x04
#define SEESAW_KEYPAD_FIFO 0x10

enum {
  SEESAW_KEYPAD_EDGE_HIGH = 0,
  SEESAW_KEYPAD_EDGE_LOW,
  SEESAW_KEYPAD_EDGE_FALLING,
  SEESAW_KEYPAD_EDGE_RISING,
};

// NeoTrellis constants
#define NEO_TRELLIS_NUM_KEYS 16
static int button_num[NEO_TRELLIS_NUM_KEYS] = { 0, 1, 2, 3, 
                                                8, 9, 10, 11,
                                                16, 17, 18, 19,
                                                24, 25, 26, 27};

typedef struct{
    uint8_t EDGE: 2;
    uint8_t NUM: 6;
} keyEvent;

union keyState {
    struct {
        uint8_t STATE : 1;  ///< the current state of the key
        uint8_t ACTIVE : 4; ///< the registered events for that key
    } bit;
    uint8_t reg;
};

typedef void (*TrellisCallback)(keyEvent evt);
TrellisCallback (*_callbacks[NEO_TRELLIS_NUM_KEYS])(keyEvent);

uint8_t neotrellis_key_finder(uint8_t key_num) {
    return ((key_num/8) * 4 + (key_num%8));
}

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

bool seesaw_read(uint8_t regHigh, uint8_t regLow, uint8_t *buf, uint8_t num, uint16_t delay) {
    uint8_t pos = 0;
    uint8_t prefix[2];
    prefix[0] = (uint8_t)regHigh;
    prefix[1] = (uint8_t)regLow;

    while (pos<num) {
        uint8_t read_now = MIN(32,num-pos);
        if(i2c_write_blocking(I2C_PORT, NEOTRELLIS_ADDR, prefix, 2, false) < 0)
            return false;
        sleep_us(delay);
        if(i2c_read_blocking(I2C_PORT, NEOTRELLIS_ADDR, buf + pos, read_now, false) < 0)
            return false;
            
        pos+=read_now;
    }
    return true;
}
uint8_t getCount(void) {
    uint8_t count = 0;

    // From Arduino src code 
    seesaw_read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &count, 1, 1000);

    return count;
}

bool readKeypad(keyEvent *buf, uint8_t count) {
    uint8_t buffer[4]={0};
    seesaw_read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_FIFO, (uint8_t*)buf, count, 1000);
    // seesaw_read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_FIFO, buffer, count, 1000);
    // buf[count].NUM = buffer[0] >> 2;
    // buf[count].EDGE = buffer[0] &0x3;
    printf("\n");
    // printf("\nbuffer %d, %d", buf[count].NUM, buf[count].EDGE);
    return true;
}

int neo_read(){
    uint8_t count = getCount();
    sleep_us(500);

    if (count > 0){
        count += 2;
        keyEvent e[count];
        readKeypad(e, count);
        for (int i = 0; i<count; i++) {
            e[i].NUM = neotrellis_key_finder(e[i].NUM);
            if(e[i].NUM < NEO_TRELLIS_NUM_KEYS && _callbacks[e[i].NUM] != NULL){
                keyEvent evt = {e[i].EDGE, e[i].NUM};
                _callbacks[e[i].NUM](evt);
            }
        }
    }

    return count;
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
    uint8_t buf = 0x01;
    seesaw_write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_INTENSET, &buf, 1);
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

TrellisCallback printKey(keyEvent evt){
    if (evt.EDGE == SEESAW_KEYPAD_EDGE_RISING){
        set_pixel_color(evt.NUM, 0, 50, 100);
        printf("KEY: %d Pressed", evt.NUM);
    } else if (evt.EDGE == SEESAW_KEYPAD_EDGE_FALLING){
        set_pixel_color(evt.NUM, 0, 0 ,0);
        printf("KEY: %d Released", evt.NUM);
    }
    show_pixels();
    return 0;
}
void setKeypadEv(uint8_t key, uint8_t edge, bool en){
    union keyState ks;
    ks.bit.STATE = en;
    ks.bit.ACTIVE = (1<<edge);
    uint8_t cmd[] = {key, ks.reg};
    seesaw_write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_EVENT, cmd, 2);
}

void regCallback(uint8_t key, TrellisCallback(*cb)(keyEvent)){
    _callbacks[key] = cb;
}

int main() {
    // Initialize stdio for printf
    stdio_init_all();
    
    // Initialize I2C
    i2c_init(I2C_PORT, 400000); // 400 kHz
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
    for(int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++){
        setKeypadEv(button_num[i], SEESAW_KEYPAD_EDGE_FALLING, true);
        setKeypadEv(button_num[i], SEESAW_KEYPAD_EDGE_RISING, true);
        regCallback(i, printKey);
    }
    printf("NeoPixels initialized successfully!\n");

    // Main loop with different patterns
    while (true) {
        neo_read();
        // Clear all
        // int num_events = neo_read(events, 16);
        // for (int i = 0; i < num_events; i++) {
        //     uint8_t x, y;
        //     neotrellis_key_to_xy(events[i].NUM, &x, &y);
            
        //     const char *action = (events[i].EDGE == 1) ? "PRESSED" : "RELEASED";
        //     printf("Key %d (x:%d y:%d) %s\n", events[i].NUM, x, y, action);
        // }

        // printf("Clearing...\n");
        // clear_all_pixels();
        // sleep_ms(1000);
      
        // // Rainbow
        // printf("Rainbow...\n");
        // rainbow_pattern();
        // sleep_ms(3000);
        
        // // Chase animation
        // printf("Chase...\n");
        // for (int i = 0; i < 3; i++) {
        //     chase_animation();
        // }
    }
    
    return 0;
}