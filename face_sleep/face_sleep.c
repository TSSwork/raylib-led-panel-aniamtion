#include "face_sleep.h"
#include <math.h>

static Face sleep_target(float t) {
    (void)t;
    Face f = face_default();
    f.eyeH = 10;
    f.mouthW = 0;
    f.r = 80; f.g = 100; f.b = 140;
    return f;
}

static void sleep_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)st; (void)blink;
    Color col = face_color(f);
    float cx = ox + FRAME_W * 0.5f;
    float cy = oy + FRAME_H * 0.5f;

    draw_eyes(f, 1.0f, 0, ox, oy);

    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 0.35f + i / 3.0f, 1.0f);
        DrawText("Z", (int)(cx + 150 + p * 90), (int)(cy - 80 - p * 160),
                 (int)(24 + p * 30), Fade(col, 1.0f - p));
    }
}

const Animation anim_sleep = { "Sleep", sleep_target, sleep_draw };
