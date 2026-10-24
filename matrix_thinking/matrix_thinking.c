#include "matrix_thinking.h"
#include <math.h>

#define ORB_SIZE  480.0f
#define ORB_DOTS  11
#define ORB_SCALE 0.92f

static const Color ORB_COLOR = { 247, 80, 1, 255 };   // #F75001

typedef struct { float radius, speed, phase, spread; } Orbiter;

static const Orbiter ORBITERS[3] = {
    { 0.62f,  2.2f,  0.0f, 0.42f },
    { 0.40f, -1.7f,  2.1f, 0.36f },
    { 0.80f,  1.15f, 4.0f, 0.34f },
};

static float thinking_intensity(float d, float nx, float ny, float t) {
    (void)d;
    float heat = 0.0f;
    for (int i = 0; i < 3; i++) {
        const Orbiter *o = &ORBITERS[i];
        float a = t * o->speed + o->phase;
        float dx = nx - cosf(a) * o->radius;
        float dy = ny - sinf(a) * o->radius;
        heat += expf(-(dx * dx + dy * dy) / (o->spread * o->spread));
    }
    return 0.26f + 0.8f * fminf(1.0f, heat);
}

// The face parameters are not used by the orb, but main.c still eases them.
static Face matrix_thinking_target(float t) {
    (void)t;
    return face_default();
}

static void matrix_thinking_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)f; (void)blink;
    float enter = 1.0f - expf(-st * 7.0f);   // grows in when the state starts
    draw_dot_matrix(ox, oy, ORB_SIZE, ORB_DOTS, ORB_SCALE * enter, t, ORB_COLOR, thinking_intensity);
}

const Animation anim_matrix_thinking = { "Orb Thinking", matrix_thinking_target, matrix_thinking_draw };
