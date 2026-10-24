#include "aurora.h"
#include <math.h>
#include <stdbool.h>

#define TAU_F          (2.0f * PI)
#define COLUMN_W       4        // pixels per column, use 2 for smoother edges or 8 for a lighter load
#define LAYERS         2        // 1 is the lightest, 3 is the maximum
#define FADE_STEPS     10       // gradient slices on the soft lower edge
#define AURORA_HEIGHT  0.45f    // how far the curtain hangs down, 0 to 1
#define AURORA_AMP     1.0f     // how strongly the edge waves
#define AURORA_BLEND   0.5f     // softness of the lower edge
#define AURORA_SCALE   1.0f     // horizontal size of the waves
#define AURORA_GLOW    0.6f
#define AURORA_SPEED   1.0f
#define INTRO_SECONDS  2.4f
#define LUT_N          128
#define STOP_COUNT     3

// #5227FF, #7cff67, #5227FF
static const Color STOPS[STOP_COUNT] = {
    { 82, 39, 255, 255 }, { 124, 255, 103, 255 }, { 82, 39, 255, 255 }
};

static Color lut[LUT_N];
static bool lutReady = false;

// ---------- Simplex noise, the same algorithm as the original shader ----------

static float mod_pos(float a, float m) { return a - m * floorf(a / m); }
static float permute1(float x) { return mod_pos((x * 34.0f + 1.0f) * x, 289.0f); }

static float snoise(float vx, float vy) {
    const float Cx = 0.211324865405187f, Cy = 0.366025403784439f;
    const float Cz = -0.577350269189626f, Cw = 0.024390243902439f;

    float s = (vx + vy) * Cy;
    float ix = floorf(vx + s), iy = floorf(vy + s);
    float t = (ix + iy) * Cx;
    float x0x = vx - ix + t, x0y = vy - iy + t;

    float i1x = x0x > x0y ? 1.0f : 0.0f;
    float i1y = x0x > x0y ? 0.0f : 1.0f;

    float x12x = x0x + Cx - i1x, x12y = x0y + Cx - i1y;
    float x12z = x0x + Cz,       x12w = x0y + Cz;

    ix = mod_pos(ix, 289.0f);
    iy = mod_pos(iy, 289.0f);

    float py[3]  = { iy, iy + i1y, iy + 1.0f };
    float offx[3] = { 0.0f, i1x, 1.0f };
    float p[3];
    for (int k = 0; k < 3; k++) p[k] = permute1(permute1(py[k]) + ix + offx[k]);

    float m[3] = {
        fmaxf(0.5f - (x0x * x0x + x0y * x0y), 0.0f),
        fmaxf(0.5f - (x12x * x12x + x12y * x12y), 0.0f),
        fmaxf(0.5f - (x12z * x12z + x12w * x12w), 0.0f),
    };
    float a0[3], h[3];
    for (int k = 0; k < 3; k++) {
        m[k] = m[k] * m[k];
        m[k] = m[k] * m[k];
        float v = p[k] * Cw;
        float x = 2.0f * (v - floorf(v)) - 1.0f;
        h[k] = fabsf(x) - 0.5f;
        a0[k] = x - floorf(x + 0.5f);
        m[k] *= 1.79284291400159f - 0.85373472095314f * (a0[k] * a0[k] + h[k] * h[k]);
    }
    float g0 = a0[0] * x0x  + h[0] * x0y;
    float g1 = a0[1] * x12x + h[1] * x12y;
    float g2 = a0[2] * x12z + h[2] * x12w;
    return 130.0f * (m[0] * g0 + m[1] * g1 + m[2] * g2);
}

// ---------- Color ramp in OKLCH, built once into a lookup table ----------

typedef struct { float L, C, h; } Lch;

static float to_lin(float c) { return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f); }
static float to_gamma(float c) {
    c = clampf(c, 0.0f, 1.0f);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

static Lch color_to_lch(Color c) {
    float r = to_lin(c.r / 255.0f), g = to_lin(c.g / 255.0f), b = to_lin(c.b / 255.0f);
    float l = cbrtf(0.4122214708f * r + 0.5363325363f * g + 0.0514459929f * b);
    float m = cbrtf(0.2119034982f * r + 0.6806995451f * g + 0.1073969566f * b);
    float s = cbrtf(0.0883024619f * r + 0.2817188376f * g + 0.6299787005f * b);
    float L = 0.2104542553f * l + 0.793617785f * m - 0.0040720468f * s;
    float A = 1.9779984951f * l - 2.428592205f * m + 0.4505937099f * s;
    float B = 0.0259040371f * l + 0.7827717662f * m - 0.808675766f * s;
    return (Lch){ L, hypotf(A, B), atan2f(B, A) };
}

static Color lch_to_color(Lch c) {
    float a = c.C * cosf(c.h), b = c.C * sinf(c.h);
    float l = c.L + 0.3963377774f * a + 0.2158037573f * b;
    float m = c.L - 0.1055613458f * a - 0.0638541728f * b;
    float s = c.L - 0.0894841775f * a - 1.2914855480f * b;
    l = l * l * l; m = m * m * m; s = s * s * s;
    float r  =  4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
    float g  = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
    float bl = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
    return (Color){ (unsigned char)(to_gamma(r) * 255.0f), (unsigned char)(to_gamma(g) * 255.0f),
                    (unsigned char)(to_gamma(bl) * 255.0f), 255 };
}

static void build_lut(void) {
    Lch lch[STOP_COUNT];
    for (int i = 0; i < STOP_COUNT; i++) lch[i] = color_to_lch(STOPS[i]);

    // Keep hue steps under half a turn so the blend takes the short way around
    float prev = lch[0].h;
    for (int i = 0; i < STOP_COUNT; i++) {
        float h = lch[i].h;
        while (h - prev > PI)  h -= TAU_F;
        while (h - prev < -PI) h += TAU_F;
        prev = h;
        lch[i].h = h;
    }

    for (int n = 0; n < LUT_N; n++) {
        float x = (float)n / (LUT_N - 1) * (STOP_COUNT - 1);
        int i = (int)fminf(floorf(x), (float)(STOP_COUNT - 2));
        float f = x - i;
        f = f * f * (3.0f - 2.0f * f);
        Lch c = { lch[i].L + (lch[i + 1].L - lch[i].L) * f,
                  lch[i].C + (lch[i + 1].C - lch[i].C) * f,
                  lch[i].h + (lch[i + 1].h - lch[i].h) * f };
        lut[n] = lch_to_color(c);
    }
    lutReady = true;
}

// ---------- Drawing ----------

static float smoother(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// The face parameters are not used by the aurora, but main.c still eases them.
static Face aurora_target(float t) {
    (void)t;
    return face_default();
}

static void aurora_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)f; (void)t; (void)blink;
    if (!lutReady) build_lut();

    float clock = st * AURORA_SPEED;
    float r = clampf(st / INTRO_SECONDS, 0.0f, 1.0f);
    float reveal = 1.0f - powf(1.0f - r, 3.0f);
    float fadeIn = smoother(0.0f, 0.7f, reveal);
    float expo = 1.5f + AURORA_GLOW * 0.6f;

    for (int layer = 0; layer < LAYERS; layer++) {
        float seed = layer * 13.7f;
        float pace = clock * (1.0f - layer * 0.18f);
        float soft = (0.04f + AURORA_BLEND * 0.16f) * (1.0f + layer * 0.9f);
        float low  = -soft - AURORA_GLOW * 0.14f;
        float high = 0.26f + soft * 0.6f;
        float layerMul = (layer == 0 ? 1.0f : 0.32f / layer) * fadeIn;

        for (int px = 0; px < FRAME_W; px += COLUMN_W) {
            float mid = px + COLUMN_W * 0.5f;
            float X = (mid - FRAME_W * 0.5f) / FRAME_H;   // units of frame height, like the original
            float u = mid / FRAME_W;

            // Height of the curtain at this column, from two layers of noise
            float warp = snoise(X * 0.45f * AURORA_SCALE + pace * 0.02f + seed, pace * 0.05f + seed);
            float wave = snoise(X * 0.75f * AURORA_SCALE + warp * 0.35f - pace * 0.03f + seed,
                                pace * 0.07f - seed) * 0.72f
                       + snoise(X * 1.6f * AURORA_SCALE + warp * 0.5f + pace * 0.04f - seed,
                                pace * 0.1f + seed * 0.5f) * 0.28f;
            float hem = AURORA_HEIGHT + layer * 0.07f + wave * 0.15f * AURORA_AMP;
            hem = -0.25f + (hem + 0.25f) * reveal;     // the curtain slides down on start

            float lobes = 0.86f + 0.14f * snoise(X * 0.9f * AURORA_SCALE - pace * 0.025f + seed,
                                                 hem * 0.3f + pace * 0.04f);
            float mul = layerMul * lobes;

            float hueU = u + 0.05f * sinf(pace * 0.08f + layer * 2.3f) + layer * 0.1f;
            Color tint = lut[(int)(clampf(hueU, 0.0f, 1.0f) * (LUT_N - 1))];

            float yFull = hem - high;   // above this the curtain is fully solid
            float yZero = hem - low;    // below this nothing is left
            if (yZero <= 0.0f) continue;

            // Solid top part
            if (yFull > 0.0f) {
                int h = (int)(fminf(yFull, 1.0f) * FRAME_H) + 1;
                DrawRectangle(ox + px, oy, COLUMN_W, h, Fade(tint, mul));
            }

            // Soft lower edge as a short stack of gradient slices
            for (int k = 0; k < FADE_STEPS; k++) {
                float y0 = yFull + (yZero - yFull) * k / FADE_STEPS;
                float y1 = yFull + (yZero - yFull) * (k + 1) / FADE_STEPS;
                if (y1 <= 0.0f || y0 >= 1.0f) continue;

                float a0 = powf(smoother(low, high, hem - y0), expo) * mul;
                float a1 = powf(smoother(low, high, hem - y1), expo) * mul;
                int top = (int)floorf(y0 * FRAME_H);
                int bot = (int)ceilf(y1 * FRAME_H);
                DrawRectangleGradientV(ox + px, oy + top, COLUMN_W, bot - top,
                                       Fade(tint, a0), Fade(tint, a1));
            }
        }
    }
}

const Animation anim_aurora = { "Aurora", aurora_target, aurora_draw };
