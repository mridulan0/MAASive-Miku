#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "neotrellis.h"

/*! \brief Initialize I2C, corresponding SDA, SCK pins
*/
void init_i2c(){
    i2c_init(I2C_PORT, 400000); // 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

/*! \brief Find corresponding trellis key number on the keypad given key index
*/
uint8_t neotrellis_key_finder(uint8_t key_num) {
    return ((key_num/8) * 4 + (key_num%8));
}

/*! \brief Write data to Seesaw register
    \param regHigh higher precedence register address for data source 
    \param regLow lower precedence register address for data source
    \param data data to write to the register
    \param len length of data in bytes
    \return 0 if data is written successfully
*/
static int seesaw_write(uint8_t regHigh, uint8_t regLow, uint8_t *data, uint8_t len) {
    uint8_t buf[34];
    buf[0] = regHigh;
    buf[1] = regLow;
    
    for (int i = 0; i < len; i++) {
        buf[2 + i] = data[i];
    }
    
    int result = i2c_write_blocking(I2C_PORT, NEOTRELLIS_ADDR, buf, len + 2, false);
    sleep_ms(5); // Increased delay for more reliable communication
    
    return result > 0 ? 0 : -1;
}

/*! \brief Read data from Seesaw
    \param regHigh higher precedence register address for data source 
    \param regLow lower precedence register address for data source
    \param buf address to write to
    \param len length of data to read
    \param delay time to wait between reading and writing from I2C 
    \return if data is read successfully
*/
static bool seesaw_read(uint8_t regHigh, uint8_t regLow, uint8_t *buf, uint8_t len, uint16_t delay) {
    uint8_t pos = 0;
    uint8_t prefix[2];
    prefix[0] = (uint8_t)regHigh;
    prefix[1] = (uint8_t)regLow;

    while (pos<len) {
        uint8_t read_now = MIN(32,len-pos);
        if(i2c_write_blocking(I2C_PORT, NEOTRELLIS_ADDR, prefix, 2, false) < 0)
            return false;
        sleep_us(delay);
        if(i2c_read_blocking(I2C_PORT, NEOTRELLIS_ADDR, buf + pos, read_now, false) < 0)
            return false;
            
        pos+=read_now;
    }
    return true;
}

/*! \brief Get event count from Seesaw count 
    \return Number of events occurred from last poll
*/
static uint8_t getCount(void) {
    uint8_t count = 0;

    // From Arduino src code 
    seesaw_read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_COUNT, &count, 1, 1000);

    return count;
}

/*! \brief Read from keypad fifo register
    \param buf bufferr to write the data read to
    \param count value from count register
    \return if fifo is successfully read
*/
bool readKeypad(keyEvent *buf, uint8_t count) {
    return seesaw_read(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_FIFO, (uint8_t*)buf, count, 1000);
    
    // printf("\nbuffer %d, %d", buf[count].NUM, buf[count].EDGE);
}

/*! \brief continuously poll keypad for events, call associated function when event detected
*/
void neo_read(){
    // Get event count from count register
    uint8_t count = getCount();
    sleep_us(500);

    if (count > 0){
        count += 2; //because no interrupt pin

        // Creates keyEvent array with "count" number of events 
        keyEvent e[count];

        // Read the corresponding events from the FIFO register
        readKeypad(e, count);
        for (int i = 0; i<count; i++) {

            // Convert weird keypad number to key index
            e[i].NUM = neotrellis_key_finder(e[i].NUM);

            // If valid key, and corresponding fxn exists, run it
            if(e[i].NUM < NEO_TRELLIS_NUM_KEYS && _callbacks[e[i].NUM] != NULL){
                keyEvent evt = {e[i].EDGE, e[i].NUM};
                _callbacks[e[i].NUM](evt);
            }
        }
    }
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
    uint16_t buf_len = NEO_TRELLIS_NUM_KEYS * 3;  
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

/*! \brief Makeshift "interrupt" function to test keypad input
    \param evt event to manipulate
*/
TrellisCallback printKey(keyEvent evt){
    if (evt.EDGE == SEESAW_KEYPAD_EDGE_RISING){
        set_pixel_color(evt.NUM, 0, 50, 100);
        printf("KEY: %d Pressed", evt.NUM);
        printf("\n");
    } else if (evt.EDGE == SEESAW_KEYPAD_EDGE_FALLING){
        set_pixel_color(evt.NUM, 0, 0 ,0);
        printf("KEY: %d Released", evt.NUM);
        printf("\n");
    }
    show_pixels();
    return 0;
}

/*! \brief Enable/disable key event
    \param key the corresponding key number
    \param edge the edge event to enable
    \param en enable or disable
*/
void setKeypadEv(uint8_t key, uint8_t edge, bool en){
    union keyState ks;
    ks.bit.STATE = en;
    ks.bit.ACTIVE = (1<<edge);
    uint8_t cmd[] = {key, ks.reg};
    seesaw_write(SEESAW_KEYPAD_BASE, SEESAW_KEYPAD_EVENT, cmd, 2);
}

/*! \brief Set corresponding key function
    \param key key index
    \param cb the "interrupt" function to set
*/
void regCallback(uint8_t key, TrellisCallback(*cb)(keyEvent)){
    _callbacks[key] = cb;
}

/*! \brief Initialize keypad event detection
    \param cb the "interrupt" function to set
*/
void init_keypad(TrellisCallback(*cb)(keyEvent)){
    for(int i = 0; i < NEO_TRELLIS_NUM_KEYS; i++){
        setKeypadEv(button_num[i], SEESAW_KEYPAD_EDGE_FALLING, true);
        setKeypadEv(button_num[i], SEESAW_KEYPAD_EDGE_RISING, true);
        regCallback(i, cb);
    }
}