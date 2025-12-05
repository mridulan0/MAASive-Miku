MAASIVE MIKU — A Keypad Rhythm Game (ECE 362 Project)
A fast-paced rhythm game built on the RP2350 “Proton” microcontroller, using the Adafruit NeoTrellis for LED/keypad input, DMA-driven audio playback, SPI LCD graphics, and ADC-based BPM control.

Overview:
MAASIVE MIKU is a 30-second rhythm game where LEDs flash in sync with a song, and the player taps the correct NeoTrellis keys in time. The project combines real-time audio, LED animations, input handling, and embedded timing logic to create a smooth, arcade-style experience.
The game provides a final score based on accuracy and gives visual feedback on each tap (yellow for correct, red for missed). A 3D-printed keypad case was designed to make the controller easier to hold during gameplay.

Features
Custom I²C driver for communicating with the Adafruit Seesaw co-processor
16 NeoPixel LEDs on the NeoTrellis controlled with RGB patterns and animations
Keypad event handling using Seesaw FIFO event queue (rising/falling edge detection)
DMA-driven PWM audio playback for smooth streaming of WAV samples
SPI LCD display showing game state, score, and animations
ADC input for adjustable volume
3D-printed enclosure for improved ergonomics

How to Play:
When the firmware starts, a short LED animation plays.
The song begins, and LEDs pulse to indicate which keys to hit.
Hit the correct button on time → LED fades yellow.
Miss the timing → LED fades red.
The game runs for ~30 seconds and displays your final score at the end.

Hardware Used:
Purdue Proton - similar to Raspberry Pi RP2350 (Proton) Microcontroller
Adafruit NeoTrellis 4×4 Keypad
SPI LCD Display
External Speaker
Potentiometer for BPM tuning
3D-printed keypad enclosure

Poster:
https://drive.google.com/file/d/1bfQbt669Cp3s1hcsxVYgpVEq_u83M8fB/view?usp=sharing

Team:
Aishani Sakalabhaktula, Amber Lu, Mridula Naikawadi, Sarah Sewell
ECE 362 – Team 34
