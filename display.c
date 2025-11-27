#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// I2C settings (RP2350B)
#define I2C_PORT i2c0
#define I2C_SDA_PIN 28
#define I2C_SCL_PIN 29
#define I2C_BAUD 400000

// Try both common seesaw addresses
#define SEESAW_ADDR_A 0x2E
#define SEESAW_ADDR_B 0x2F

// Seesaw keypad registers (as used earlier)
#define SEESAW_KEYPAD_BASE 0x10
#define SEESAW_KEYPAD_FIFO (SEESAW_KEYPAD_BASE + 0x10)
#define SEESAW_KEYPAD_COUNT (SEESAW_KEYPAD_BASE + 0x04)
#define SEESAW_KEYPAD_INTEN (SEESAW_KEYPAD_BASE + 0x0E)

// Event edge bits
#define EDGE_RISING  0x01
#define EDGE_FALLING 0x02

// Adafruit funky keymap (index -> raw value)
static const uint8_t trellis_key_map[16] = {
    0, 1, 2, 3,
    8, 9, 10, 11,
    16, 17, 18, 19,
    24, 25, 26, 27
};

static uint8_t seesaw_addr = 0;

// Low-level write (write two-byte register [base, reg] then data)
static int seesaw_write_bytes(uint8_t addr, uint8_t base, uint8_t reg, const uint8_t *data, size_t len) {
    uint8_t buf[2 + len];
    buf[0] = base;
    buf[1] = reg;
    if (len) memcpy(&buf[2], data, len);
    return i2c_write_blocking(I2C_PORT, addr, buf, 2 + len, false);
}

// Low-level read (write 2 byte register then read)
static int seesaw_read_bytes(uint8_t addr, uint8_t base, uint8_t reg, uint8_t *dst, size_t len) {
    uint8_t regbuf[2] = { base, reg };
    int w = i2c_write_blocking(I2C_PORT, addr, regbuf, 2, true);
    if (w < 0) return w;
    return i2c_read_blocking(I2C_PORT, addr, dst, len, false);
}

// Scan I2C bus for the seesaw device
static uint8_t i2c_scan_for_seesaw() {
    printf("Scanning I2C bus for Seesaw device...\n");
    for (int a = 0x08; a <= 0x77; a++) {
        // attempt a zero-length read (write address then read 0 bytes) to detect ack
        int ret = i2c_read_blocking(I2C_PORT, a, NULL, 0, false);
        // i2c_read_blocking returns number of bytes read or PICO_ERROR_GENERIC on NAK
        // But driver often returns a negative on NAK — simpler approach: try a small write
        
        uint8_t dummy = 0;
        int w = i2c_write_blocking(I2C_PORT, a, &dummy, 0, false);
        if (w >= 0) {
            // We found a responding address
            printf("I2C device found at 0x%02X\n", a);
            if (a == SEESAW_ADDR_A || a == SEESAW_ADDR_B) {
                printf(" - Matches expected Seesaw address candidate\n");
                return (uint8_t)a;
            }
        }
    }
    // fallback: check the two most-likely explicitly
    // Try a read of COUNT at 0x2E and 0x2F to see which ACKs
    uint8_t buf;
    if (seesaw_read_bytes(SEESAW_ADDR_A, SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &buf, 1) >= 0) return SEESAW_ADDR_A;
    if (seesaw_read_bytes(SEESAW_ADDR_B, SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &buf, 1) >= 0) return SEESAW_ADDR_B;
    return 0;
}

// Initialize I2C and keypad (enable events mask for completeness)
static void init_hw() {
    stdio_init_all();
    i2c_init(I2C_PORT, I2C_BAUD);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    sleep_ms(10);
}

// Setup keypad int enable mask (we still poll so this is just to ensure events are produced)
static void keypad_enable_all(uint8_t addr) {
    // INTEN expects 2 bytes mask for 16 keys (LSB first). We set all bits = enabled.
    uint8_t inten[2] = { 0xFF, 0xFF };
    int r = seesaw_write_bytes(addr, SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_INTEN, inten, 2);
    if (r < 0) {
        printf("Warning: failed to write INTEN (err %d)\n", r);
    } else {
        printf("Wrote INTEN all-ones\n");
    }
}

// Print bytes in hex
static void print_hex(const uint8_t *buf, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        printf("%02X ", buf[i]);
    }
}

// Try to map raw to index using the funky map, return -1 if not found
static int map_raw_to_index(uint8_t raw) {
    for (int i = 0; i < 16; ++i) if (trellis_key_map[i] == raw) return i;
    return -1;
}
void keypad_init(uint8_t addr) {
    // Enable ROWS (4 rows => 0b1111)
    uint8_t rows = 0x0F;
    seesaw_write_bytes(addr, SEESAW_KEYPAD_BASE, 0x20, &rows, 1);

    // Enable COLUMNS (4 columns => 0b1111)
    uint8_t cols = 0x0F;
    seesaw_write_bytes(addr, SEESAW_KEYPAD_BASE, 0x21, &cols, 1);

    // Enable the keypad engine
    uint8_t enable = 0x01;
    seesaw_write_bytes(addr, SEESAW_KEYPAD_BASE, 0x02, &enable, 1);

    // Enable rising + falling events for all 16 keys
    uint8_t inten[2] = {0xFF, 0xFF};
    seesaw_write_bytes(addr, SEESAW_KEYPAD_BASE, 0x0E, inten, 2);

    sleep_ms(10);
}

int main() {
    init_hw();
    keypad_init(seesaw_addr);
    // find device
    seesaw_addr = i2c_scan_for_seesaw();
    if (seesaw_addr == 0) {
        printf("Seesaw device not found at common addresses. Trying 0x2E/0x2F explicitly...\n");
        // fallback to try both
        uint8_t test;
        if (seesaw_read_bytes(SEESAW_ADDR_A, SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &test, 1) >= 0) {
            seesaw_addr = SEESAW_ADDR_A;
        } else if (seesaw_read_bytes(SEESAW_ADDR_B, SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &test, 1) >= 0) {
            seesaw_addr = SEESAW_ADDR_B;
        }
    }

    if (seesaw_addr == 0) {
        printf("ERROR: no Seesaw device detected. Check wiring/power and addresses.\n");
        while (1) sleep_ms(1000);
    }

    printf("Using Seesaw I2C address 0x%02X\n", seesaw_addr);
    keypad_enable_all(seesaw_addr);
    sleep_ms(20);

    printf("Starting polling loop (COUNT -> read FIFO block)...\n");

    while (1) {
        uint8_t count = 0;
        int r = seesaw_read_bytes(seesaw_addr, SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &count, 1);
        if (r < 0) {
            printf("I2C error reading COUNT: %d\n", r);
            sleep_ms(200);
            continue;
        }

        if (count == 0) {
            // nothing pending
            sleep_ms(10);
            continue;
        }

        if (count > 8) {
            printf("COUNT reported >8 (%u). Clamping to 8.\n", count);
            count = 8;
        }

        printf("COUNT = %u -> reading %u events (%u bytes)\n", count, (unsigned)count, (unsigned)(count * 4));

        // Read all events in one block
        size_t bytes_to_read = (size_t)count * 4;
        uint8_t fifo_buf[32];
        if (seesaw_read_bytes(seesaw_addr, SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_FIFO, fifo_buf, bytes_to_read) < 0) {
            printf("Error reading FIFO\n");
            sleep_ms(20);
            continue;
        }

        printf("FIFO raw bytes: ");
        print_hex(fifo_buf, bytes_to_read);
        printf("\n");

        // Process each 4-byte event
        for (size_t ev = 0; ev < count; ++ev) {
            uint8_t *e = &fifo_buf[ev * 4];
            uint8_t head = e[0];
            uint8_t key_raw = head >> 2;
            uint8_t edge = head & 0x03;

            int key_index = map_raw_to_index(key_raw);

            // Debug: print full event details
            printf("Event %u: head=0x%02X raw=%u edge=%u bytes=", (unsigned)ev, head, (unsigned)key_raw, (unsigned)edge);
            print_hex(e, 4);
            printf(" -> ");

            if (key_index >= 0) {
                if (edge == EDGE_RISING) printf("KEY %d PRESSED\n", key_index);
                else if (edge == EDGE_FALLING) printf("KEY %d RELEASED\n", key_index);
                else printf("KEY %d EDGE=0x%02X\n", key_index, edge);
            } else {
                // Unknown raw value: print helpful alternatives
                printf("UNKNOWN raw=%u. (head>>2=%u). Trying fallback guesses: index_guess=%u\n",
                       (unsigned)key_raw, (unsigned)key_raw, (unsigned)(head & 0x0F));
                printf(" - full head bits: ");
                for (int b = 7; b >= 0; --b) printf("%d", (head >> b) & 1);
                printf("\n");
            }
        }
        // allow keyboard debounce and reduce bus load
        sleep_ms(20);
    }
}
