#ifndef BMP_H
#define BMP_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lcd.h"

// A simple header for our raw files.
typedef struct {
    uint32_t width;
    uint32_t height;
} RawImageHeader;

// Make it a nice struct with all the attributes...
typedef struct {
    unsigned int   width;
    unsigned int   height;
    unsigned int   bytes_per_pixel;
    unsigned char *pixel_data;
} MutablePicture;

Picture* load_image(const uint8_t* raw_data);
void free_image(Picture* pic);
#endif