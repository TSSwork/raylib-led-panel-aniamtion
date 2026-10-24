#ifndef COMMON_H
#define COMMON_H

#include "raylib.h"

#define SIDEBAR_W 200
#define FRAME_W   800
#define FRAME_H   600
#define WIN_W     (SIDEBAR_W + FRAME_W)
#define WIN_H     FRAME_H

// All fields are floats so a whole face can be eased smoothly between states.
typedef struct {
    float eyeW, eyeH, gap;
    float lookX, lookY;
    float mouthW, mouthCurve, mouthOpen;
    float r, g, b;
} Face;

// One animation state. Each face file provides one of these.
typedef struct {
    const char *name;                 // text shown on the sidebar button
    Face (*target)(float t);          // face parameters to ease toward
    void (*draw)(const Face *f, float t, float st, float blink, int ox, int oy);
} Animation;

Face  face_default(void);
void  face_ease(Face *cur, const Face *tgt, float dt);
Color face_color(const Face *f);

// Shared drawing helpers, coordinates are in 800x600 space offset by (ox, oy).
void draw_eyes(const Face *f, float blink, float shake, int ox, int oy);
void draw_mouth(const Face *f, float shake, int ox, int oy);

#endif
