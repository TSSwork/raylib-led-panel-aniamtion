#include "face_listening.h"
#include <math.h>

static Face listening_target(float t) {
    (void)t;
    Face f = face_default();
    f.eyeW = 145; f.eyeH = 185;
    f.mouthW = 60; f.mouthCurve = 0;
    f.r = 120; f.g = 255; f.b = 170;
    return f;
}

static void listening_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)st;
    Color col = face_color(f);
    Vector2 c = { ox + FRAME_W * 0.5f, oy + FRAME_H * 0.5f };
    for (int k = 0; k < 3; k++) {
        float p = fmodf(t * 0.7f + k / 3.0f, 1.0f);
        DrawRing(c, 250 + p * 200, 256 + p * 200, 0, 360, 96, Fade(col, 1.0f - p));
    }
    draw_eyes(f, blink, 0, ox, oy);
    draw_mouth(f, 0, ox, oy);
}

const Animation anim_listening = { "Listening", listening_target, listening_draw };
