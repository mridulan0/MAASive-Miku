// TFT LCD Display has 9 pins
// Use SPI1

// VCC-     3.3~5V
// GND-     ground
// CS LCD select    GP9
// Reset    GP15
// DC/RS    GP8
// SDI/MOSI GP11
// SCK-     GP10
// LED - connect to 3.3V (I am not doing backlight ctrl fgs)
// SDO/MISO GP12
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"

#define TFT_CSx 9
#define TFT_RST 15
#define TFT_DC 8
#define TFT_MOSI 11
#define TFT_SCK 10
#define TFT_MISO 12

void tft_init(int spi_clk_freq){
    //initialize spi
    spi_init(spi1, spi_clk_freq);
    uint baudrate = spi_set_baudrate(spi1, spi_clk_freq);
    
    //initialize pins to spi
    gpio_set_function(TFT_MISO, GPIO_FUNC_SPI);
    gpio_set_function(TFT_SCK, GPIO_FUNC_SPI);
    gpio_set_function(TFT_MOSI, GPIO_FUNC_SPI);

    //initialize reset, chip select and DS pins
    gpio_init(TFT_RST);
    gpio_set_dir(TFT_RST, true);
    gpio_put(TFT_RST, 1);

    gpio_init(TFT_CSx);
    gpio_set_dir(TFT_CSx, true);
    gpio_put(TFT_CSx, 1);

    gpio_init(TFT_DC);
    gpio_set_dir(TFT_DC, true);
    gpio_put(TFT_DC, 0);

    sleep_ms(10);
    gpio_put(TFT_RST, 0);

    sleep_ms(10);
    gpio_put(TFT_RST, 1);

    sleep_us(1);
    gpio_put(TFT_CSx, true);
    sleep_us(1);
    gpio_put(TFT_DC, 0);
}