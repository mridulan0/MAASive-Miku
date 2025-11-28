// game.h
#ifndef MINIGAME_H
#define MINIGAME_H

#include <stdint.h>
#include "neotrellis.h"   // for keyEvent, TrellisCallback, etc.

// Initialize game state, keypad callback, timers, etc.
void game_init(void);

// Run one "step" of the game (called repeatedly from main loop)
void game_step(void);

// Optional: expose score if you want to print it from main later
int game_get_combo(void);

#endif
