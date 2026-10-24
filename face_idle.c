#include "face_idle.h"

// Private state for the random glances.
static float gazeX = 0, gazeY = 0, nextGaze = 2.0f;

static Face idle_target(float t) {
    if (t > nextGaze) {
        gazeX = (float)GetRandomValue(-40, 40);
        gazeY = (float)GetRandomValue(-25, 25);
        nextGaze = t + 1.5f + GetRandomValue(0, 25) / 10.0f;
    }
    Face f = face_default();
    f.lookX = gazeX;
    f.lookY = gazeY;
    return f;
}

static void idle_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)t; (void)st;
    draw_eyes(f, blink, 0, ox, oy);
    draw_mouth(f, 0, ox, oy);
}

const Animation anim_idle = { "Idle", idle_target, idle_draw };
