#ifndef OLED_H
#define OLED_H

#include "raylib.h"
#include <stdbool.h>

// Talks to the Arduino UNO running sketch/sketch.ino over USB serial.
// If no board answers, everything here quietly does nothing.

// Call once per frame. Finds and connects to the board, and returns how many
// times the pushbutton was pressed since the last call.
int  oled_poll(void);

// True when the board is connected and waiting for its next frame.
bool oled_ready(void);

// Converts an 800x600 RGBA image of the robot screen to 128x64 and sends it.
void oled_send(const Image *frame);

void oled_close(void);

#endif
