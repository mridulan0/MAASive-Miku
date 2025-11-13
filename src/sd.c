#include "pico/stdlib.h"
#include "ff.h"
#include <stdio.h>


#define C 262
#define Csharp 277
#define D 294
#define E 330
#define F 349
#define Fsharp 370
#define G 392
#define Gsharp 415
#define A 440
#define B 494
#define C2 523
#define C2sharp 554

typedef struct {
    int freq;
    int duration;
} Note;

Note song[] = {
        {Csharp, 500}, {Fsharp, 500}, {Fsharp, 750}, {Gsharp, 250}, 
        {A, 250}, {A, 250}, {Fsharp, 250}, {Fsharp, 250},
        {Fsharp, 500}, {Fsharp, 250}, {A, 250}, {Gsharp, 500},
        {E, 500}, {E, 500}, {Gsharp, 500}, {A, 500},
        {Fsharp, 500}, {Fsharp, 500}, {Fsharp, 250}, {Fsharp, 250},
        {Csharp, 500}, {Fsharp, 500}, {Fsharp, 750}, {Gsharp, 250},
        {A, 500}, {Fsharp, 500}, {Fsharp, 500}, {Fsharp, 250},
        {A, 250}, {C2sharp, 250}, {C2sharp, 250}, {C2sharp, 250},
        {B, 250}, {A, 500}, {Gsharp, 500}, {A, 500},
        {Fsharp, 500}, {Fsharp, 500}, {Fsharp, 250}, {A, 250},
        {C2sharp, 500}, {C2sharp, 500}, {B, 500}, {A, 500},
        {Gsharp, 500}, {E, 500}, {E, 500}, {E, 250},
        {Gsharp, 250}, {B, 250}, {B, 250}, {B, 250},
        {B, 250}, {A, 250}, {A, 250}, {Gsharp, 250},
        {Gsharp, 250}, {A, 500}, {Fsharp, 500}, {Fsharp, 500},
        {Fsharp, 250}, {A, 250}, {C2sharp, 500}, {C2sharp, 500}, 
        {B, 500}, {A, 500}, {Gsharp, 500}, {E, 500}, 
        {E, 500}, {E, 250}, {Gsharp, 250}, {B, 250}, 
        {B, 250}, {B, 250}, {B, 250}, {A, 250}, 
        {A, 250}, {Gsharp, 250}, {Gsharp, 250}, {A, 500}, 
        {Fsharp, 500}, {Fsharp, 750}, {Fsharp, 250}
    };

FATFS fs;
FIL file;
FRESULT res;

void write_to_sd() {
    int num_notes = sizeof(song)/sizeof(song[0]);
    int song_length = 0;

    res = f_mount(&fs, "", 1);
    if (res != FR_OK) {
        printf("Mount failed: %d\n", res);
        return;
    }

    res = f_open(&file, 'ievan_polkka.txt', FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        printf("Failed to open txt file: %d\n", res);
        return;
    }

    for (int i = 0; i < num_notes; i++) {
        fprintf(&file, "%d,%d\n", song[i].freq, song[i].duration);
    }

    f_close(&file);
    printf("Wrote %d notes to txt file!\n", num_notes);
}