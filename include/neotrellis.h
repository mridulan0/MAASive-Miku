#ifndef NEOTRELLIS_H
// NeoTrellis I2C address (default)
#define NEOTRELLIS_H
#include <stdio.h>

#define NEOTRELLIS_ADDR 0x2E

// I2C pins
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

// Keypad events
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

// Some key structs
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

//TrellisCallback printKey(keyEvent evt);
void init_i2c();
void neo_read();
int init_neopixels();
int set_pixel_color(uint8_t pixel, uint8_t r, uint8_t g, uint8_t b);
int show_pixels();
int clear_all_pixels();
void init_keypad(TrellisCallback(*cb)(keyEvent));
#endif