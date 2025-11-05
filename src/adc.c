#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/adc.h"

//////////////////////////////////////////////////////////////////////////////

const char* username = "sewell7";

//////////////////////////////////////////////////////////////////////////////

// objective: create an adc that can automatically convert between the analog
// input signal from the potentiometer to a digital signal that can be used
// by the PWM.

// ideally, this will be used to change the amplitude and frequency of the 
// song.

