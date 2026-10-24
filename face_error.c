#include "face_error.h"
#include <math.h>

static Face error_target(float t) {
    (void)t;
    Face f = face_default();
    f.eyeW = 150; f.eyeH = 70;
    f.mouthW = 150; f.mouthCurve = -0.6f;
    f.r = 255; f.g = 80; f.b = 80;
    return f;
}

static void error_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)t;
    // Shake hard at the start, then settle down.
    float shake = sinf(st * 55.0f) * 12.0f * fmaxf(0.0f, 1.0f - st * 0.8f);
    draw_eyes(f, blink, shake, ox, oy);
    draw_mouth(f, shake, ox, oy);
}

const Animation anim_error = { "Error", error_target, error_draw };
