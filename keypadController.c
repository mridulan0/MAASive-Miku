
// minimal seesaw/trellis driver (addr 0x2e) with event polling + rgb leds
// follow pseudo-code: init bus -> reset -> read events -> led control

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

//i2c + timing + logging hooks (implement for your board)  
void i2c_init(int sda_pin, int scl_pin, uint32_t hz);                // set pins, mode, pullups, speed
bool i2c_write(uint8_t addr, const uint8_t *data, size_t len);       // blocking write
bool i2c_write_read(uint8_t addr, const uint8_t *w, size_t wn, uint8_t *r, size_t rn); // write then read
void delay_ms(uint32_t ms);
void logf(const char *fmt, ...);                                     // printf-style logger

//device constants  
#define TRELLIS_I2C_ADDR    0x2E

//seesaw module bases
#define MOD_STATUS          0x00
#define MOD_NEOPIX          0x0E
#define MOD_KEYPAD          0x10

//status regs
#define REG_STATUS_SWRST    0x7F

//keypad regs (seesaw keypad engine)
#define REG_KEY_EVENT       0x01   // pop next event (2 bytes)
#define REG_KEY_INTENSET    0x02   // enable event generation (optional)
#define REG_KEY_INTENCLR    0x03
#define REG_KEY_COUNT       0x04   // pending event count (1 byte)
//note: for trellis 4x4, the built-in fw already knows the 16 keys; no row/col wiring needed

//neopixel regs
#define REG_NEO_NUM         0x00   // pixel count (2 bytes, little-endian)
#define REG_NEO_PIN         0x01   // output pin (1 byte) – trellis uses pin 3 internally
#define REG_NEO_BUF         0x03   // write rgb buffer at an offset
#define REG_NEO_SHOW        0x05   // latch/show

//trellis specifics
#define TRELLIS_PIXELS      16
#define TRELLIS_NEO_PIN     3

//low-level seesaw helpers  
static bool ss_write(uint8_t mod, uint8_t reg, const uint8_t *payload, size_t n)
{
    uint8_t buf[2 + 64];                 // header + small payloads (adjust if you stream bigger)
    if (n > sizeof(buf) - 2) return false;
    buf[0] = mod;
    buf[1] = reg;
    for (size_t i = 0; i < n; ++i) buf[2 + i] = payload ? payload[i] : 0;
    return i2c_write(TRELLIS_I2C_ADDR, buf, 2 + n);
}

static bool ss_read(uint8_t mod, uint8_t reg, uint8_t *out, size_t n)
{
    uint8_t hdr[2] = { mod, reg };
    return i2c_write_read(TRELLIS_I2C_ADDR, hdr, 2, out, n);
}

//device init / reset  
static bool trellis_reset(void)
{
    //software reset
    uint8_t val = 0xFF;
    if (!ss_write(MOD_STATUS, REG_STATUS_SWRST, &val, 1)) return false;
    delay_ms(10); //give seesaw time to reboot
    return true;
}

static bool trellis_begin_i2c(int sda_pin, int scl_pin, uint32_t hz)
{
    i2c_init(sda_pin, scl_pin, hz);  //e.g., 400000 for 400 kHz fast-mode
    delay_ms(2);
    return trellis_reset();
}

//keypad event api  
typedef struct {
    uint8_t key;     // 0..15
    bool    pressed; // bit0
    bool    released;// bit1
} trellis_event_t;

//returns number of pending events (0..255)
static uint8_t trellis_event_count(void)
{
    uint8_t c = 0;
    if (!ss_read(MOD_KEYPAD, REG_KEY_COUNT, &c, 1)) return 0;
    return c;
}

//pops one event from fifo
static bool trellis_read_event(trellis_event_t *e)
{
    uint8_t raw[2] = {0};
    if (!ss_read(MOD_KEYPAD, REG_KEY_EVENT, raw, 2)) return false;
    e->key      = raw[0];               // 0..15 on trellis
    e->pressed  = (raw[1] & 0x01) != 0; // bit0
    e->released = (raw[1] & 0x02) != 0; // bit1
    return true;
}

//helpers to map key index to (row,col)
static inline uint8_t trellis_row(uint8_t key) { return key / 4; }
static inline uint8_t trellis_col(uint8_t key) { return key % 4; }

//neopixel control  
static bool trellis_pixels_begin(void)
{
    //set pixel count (16) and pin (3).
    //fw ignores pin on this board, but it’s harmless
    uint8_t num_le[2] = { (uint8_t)(TRELLIS_PIXELS & 0xFF), (uint8_t)(TRELLIS_PIXELS >> 8) };
    if (!ss_write(MOD_NEOPIX, REG_NEO_NUM, num_le, 2)) return false;
    uint8_t pin = TRELLIS_NEO_PIN;
    if (!ss_write(MOD_NEOPIX, REG_NEO_PIN, &pin, 1)) return false;
    return true;
}

//write a single pixel’s rgb into the device buffer (0..15). call trellis_pixels_show() to latch.
static bool trellis_pixel_write(uint8_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    //buffer format: offset (2 bytes) + rgb bytes to write beginning at that offset
    //each led is 3 bytes, offset = idx*3
    uint16_t off = (uint16_t)idx * 3u;
    uint8_t buf[2 + 3];
    buf[0] = (uint8_t)(off >> 8);
    buf[1] = (uint8_t)(off & 0xFF);
    buf[2] = r; buf[3] = g; buf[4] = b;
    return ss_write(MOD_NEOPIX, REG_NEO_BUF, buf, sizeof(buf));
}

static bool trellis_pixels_fill(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint8_t i = 0; i < TRELLIS_PIXELS; ++i)
        if (!trellis_pixel_write(i, r, g, b)) return false;
    return true;
}

static bool trellis_pixels_show(void)
{
    //show has no payload
    return ss_write(MOD_NEOPIX, REG_NEO_SHOW, NULL, 0);
}

//set color by row/col
static inline bool trellis_set_rc(uint8_t row, uint8_t col, uint8_t r, uint8_t g, uint8_t b)
{
    if (row > 3 || col > 3) return false;
    uint8_t idx = row * 4 + col;
    return trellis_pixel_write(idx, r, g, b);
}

//simple bpm blinker not sure about keepig yet
static void trellis_bpm_tick(uint32_t bpm, uint32_t elapsed_ms, uint8_t r, uint8_t g, uint8_t b)
{
    //on-beat flash ~80ms each beat
    uint32_t beat_ms = (bpm == 0) ? 0 : (60000u / bpm);
    if (!beat_ms) return;
    if ((elapsed_ms % beat_ms) < 80) {
        trellis_pixels_fill(r, g, b);
    } else {
        trellis_pixels_fill(0, 0, 0);
    }
    trellis_pixels_show();
}

//polling loop i2c 
void trellis_run_polling(uint32_t delay_ms_between_polls, uint32_t bpm_for_flash_or_0)
{
    uint32_t t = 0;
    while (1) {
        //tempo feedback
        if (bpm_for_flash_or_0) {
            trellis_bpm_tick(bpm_for_flash_or_0, t, 8, 0, 12); // dim purple flash
        }

        uint8_t n = trellis_event_count();
        if (n > 0) {
            for (uint8_t i = 0; i < n; ++i) {
                trellis_event_t ev;
                if (!trellis_read_event(&ev)) break;

                uint8_t r = trellis_row(ev.key);
                uint8_t c = trellis_col(ev.key);
                if (ev.pressed) {
                    logf("key (%u,%u) pressed\n", r, c);
                    // light the key on press
                    trellis_set_rc(r, c, 0, 30, 0); // green
                    trellis_pixels_show();
                }
                if (ev.released) {
                    logf("key (%u,%u) released\n", r, c);
                    // turn it off on release or pick new color
                    trellis_set_rc(r, c, 0, 0, 0);
                    trellis_pixels_show();
                }
            }
        }

        delay_ms(delay_ms_between_polls);
        t += delay_ms_between_polls;
    }
}

//bring-up entry
bool trellis_setup_and_start(int sda_pin, int scl_pin)
{
    //1) init i2c bus (400 khz)
    if (!trellis_begin_i2c(sda_pin, scl_pin, 400000)) {
        logf("trellis: i2c init/reset failed\n");
        return false;
    }

    // 2) small wait after reset
    delay_ms(5);

    if (!trellis_pixels_begin()) {
        logf("trellis: neopixel init failed\n");
        return false;
    }
    trellis_pixels_fill(0,0,0);
    trellis_pixels_show();

    logf("trellis ready on 0x%02x\n", TRELLIS_I2C_ADDR);
    return true;
}

/*
int main(void)
{
    // choose pins for your board here
    const int SDA = 2;    // <-- replace with your sda pin
    const int SCL = 3;    // <-- replace with your scl pin

    if (!trellis_setup_and_start(SDA, SCL)) {
        while (1) { }
    }

    // poll every 8 ms; set bpm=120 for tempo flash (0 to disable)
    trellis_run_polling(8, 120);
}
*/
