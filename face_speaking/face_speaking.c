#include "face_speaking.h"
#include <math.h>

static Face speaking_target(float t) {
    (void)t;
    Face f = face_default();
    f.mouthW = 170; f.mouthCurve = 0.1f;
    return f;
}

static void speaking_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)st;
    Face g = *f;
    g.mouthOpen = fmaxf(0.0f, sinf(t * 13.0f) * sinf(t * 4.1f));
    draw_eyes(&g, blink, 0, ox, oy);
    draw_mouth(&g, 0, ox, oy);
}

const Animation anim_speaking = { "Speaking", speaking_target, speaking_draw };
