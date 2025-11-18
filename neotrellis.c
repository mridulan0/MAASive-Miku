#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <math.h>
#include <string.h>
#include "hardware/i2c.h"

#define I2C_SDA 4
#define I2C_SCL 5
#define NEOTRELLIS_ADDR 0x2E

// Seesaw base addresses
//#define SEESAW_STATUS_BASE     0x00
//#define SEESAW_NEOPIXEL_BASE   0x0E

// Status functions
#define SEESAW_STATUS_HW_ID 0x01
#define SEESAW_STATUS_SWRST 0x7F

// NeoPixel functions
#define SEESAW_NEOPIX_PIN 0x01
#define SEESAW_NEOPIX_SPEED 0x02
#define SEESAW_NEOPIX_BUF_LEN 0x03
#define SEESAW_NEOPIX_BUF 0x04
#define SEESAW_NEOPIX_SHOW 0x05

#define NEOTRELLIS_NUM_PIXELS 16
#define NEOTRELLIS_BUF_BYTES 48  // 16 * 3 = 48
#define NEOTRELLIS_NEOPIX_PIN 3

#define SEESAW_STATUS_BASE 0x00
#define SEESAW_GPIO_BASE 0x01
#define SEESAW_SERCOM0_BASE 0x02
#define SEESAW_SERCOM1_BASE 0x03
#define SEESAW_SERCOM2_BASE 0x04
#define SEESAW_SERCOM3_BASE 0x05
#define SEESAW_SERCOM4_BASE 0x06
#define SEESAW_SERCOM5_BASE 0x07
#define SEESAW_TIMER_BASE 0x08
#define SEESAW_ADC_BASE 0x09
#define SEESAW_DAC_BASE 0x0A
#define SEESAW_INTERRUPT_BASE 0x0B
#define SEESAW_DAP_BASE 0x0C
#define SEESAW_EEPROM_BASE 0x0D
#define SEESAW_NEOPIXEL_BASE 0x0E
#define SEESAW_TOUCH_BASE 0x0F
#define SEESAW_KEYPAD_BASE 0x10
#define SEESAW_ENCODER_BASE 0x11

void init_i2c() {
    // Init I2C0 at 400 kHz (Fast Mode)
    i2c_init(i2c0, 400 * 1000);

    // Assign the I2C function to SDA/SCL pins
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    // Enable internal pull-ups on SDA/SCL (required for I2C)
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL); 
}

//similar to eeprom_write
static int seesaw_write(uint8_t module, uint8_t function, const uint8_t *payload, uint8_t len) {
    uint8_t buf[2 + 32];// 2-byte header + up to 32 bytes payload
    if (len > 32) return -1;

    buf[0] = module;
    buf[1] = function;
    for (uint8_t i = 0; i < len; i++) {
        buf[2 + i] = payload[i];
    }

    int ret = i2c_write_blocking(i2c0, NEOTRELLIS_ADDR, buf, 2 + len, false);
    return ret;  // returns number of bytes written or error
}

//similar to eeprom_read
static int seesaw_read(uint8_t module, uint8_t function, uint8_t *buf, uint8_t len) {
    uint8_t header[2] = { module, function };

    // Select the register (module+function)
    int ret = i2c_write_blocking(i2c0, NEOTRELLIS_ADDR, header, 2, true); // nostop = true
    if (ret < 0) return ret;
    
    sleep_us(500); //short delay

    //read the data
    ret = i2c_read_blocking(i2c0, NEOTRELLIS_ADDR, buf, len, false);
    return ret;
}

void neotrellis_init(void) {
    uint8_t buf[4];
    //init_i2c ccalled in main -> need time to powerup
    sleep_ms(50);

    //check status
    if (seesaw_read(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, buf, 1) > 0) {
        printf("Seesaw HW_ID: 0x%02X\r\n", buf[0]);
    }

    //software reset: STATUS base, SWRST function, no payload
    uint8_t rst = 0xFF;
    seesaw_write(SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, &rst, 1);
    sleep_ms(10);

    //set NeoPixel buffer length to 48
    buf[0] = (uint8_t)(NEOTRELLIS_BUF_BYTES & 0xFF);      // LSB
    buf[1] = (uint8_t)((NEOTRELLIS_BUF_BYTES >> 8) & 0xFF); // MSB
    seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIX_BUF_LEN, buf, 2);

    //setingt NeoPixel speed to 800khz for now
    buf[0] = 0x01;  // 0 = 400kHz, 1 = 800kHz
    seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIX_SPEED, buf, 1);

    //setting NeoPixel PIN to internal pin 3 (NOT your Pico pin!)
    buf[0] = NEOTRELLIS_NEOPIX_PIN;
    seesaw_write(SEESAW_NEOPIXEL_BASE, SEESAW_NEOPIX_PIN, buf, 1);

    //check status again
    if (seesaw_read(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, buf, 1) > 0) {
        printf("Seesaw HW_ID (after init): 0x%02X\r\n", buf[0]);
    }
}

