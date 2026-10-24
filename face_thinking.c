#include "face_thinking.h"
#include <math.h>

static Face thinking_target(float t) {
    (void)t;
    Face f = face_default();
    f.lookX = -60; f.lookY = -55;
    f.eyeH = 110;
    f.mouthW = 50; f.mouthCurve = 0;
    f.r = 255; f.g = 200; f.b = 90;
    return f;
}

static void thinking_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)st;
    Color col = face_color(f);
    float cx = ox + FRAME_W * 0.5f;
    float cy = oy + FRAME_H * 0.5f;

    draw_eyes(f, blink, 0, ox, oy);
    draw_mouth(f, 0, ox, oy);

    for (int i = 0; i < 3; i++) {
        float b = fmaxf(0.0f, sinf(t * 6.0f - i * 0.9f));
        DrawCircle((int)(cx + 190 + i * 36), (int)(cy - 190 - b * 18), 11, Fade(col, 0.5f + 0.5f * b));
    }
}

const Animation anim_thinking = { "Thinking", thinking_target, thinking_draw };
