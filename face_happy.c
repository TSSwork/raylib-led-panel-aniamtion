#include "face_happy.h"

static Face happy_target(float t) {
    (void)t;
    Face f = face_default();
    f.mouthW = 190; f.mouthCurve = 0.9f;
    f.r = 255; f.g = 150; f.b = 200;
    return f;
}

static void happy_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)t; (void)st; (void)blink;
    Color col = face_color(f);
    float cx = ox + FRAME_W * 0.5f + f->lookX;
    float cy = oy + FRAME_H * 0.5f - 40 + f->lookY;

    // Eyes as upward arches
    for (int i = 0; i < 2; i++) {
        float x = cx + (i == 0 ? -f->gap * 0.5f : f->gap * 0.5f);
        DrawRing((Vector2){ x, cy + 30 }, 45, 75, 190, 350, 32, col);
    }
    draw_mouth(f, 0, ox, oy);
}

const Animation anim_happy = { "Happy", happy_target, happy_draw };
