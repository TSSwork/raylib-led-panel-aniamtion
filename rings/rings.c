#include "rings.h"
#include <math.h>
#include <stdbool.h>

#define TAU_F        (2.0f * PI)
#define RING_COUNT   6
#define BASE_RADIUS  0.35f     // all radii are in plane units, one unit equals UNIT pixels
#define RADIUS_STEP  0.10f
#define EXPANSION    0.10f
#define TAPER        1.5f      // higher means the outer rings become shorter arcs
#define UNIT         380.0f
#define LINE_W       4.0f      // core thickness in pixels
#define GLOW         1.0f
#define SPEED        1.0f
#define SPIN         0.3f      // the original default is 0, which keeps the arcs still
#define ROTATION     0.0f      // radians
#define PARALLAX     0.05f
#define HOVER_SCALE  1.2f
#define HOVER_POWER  1.0f
#define SEGMENTS     120       // slices per ring, lower this for a lighter load
#define CYCLE        3.45f
#define STAGGER      0.5f
#define MAX_BURSTS   3
#define BURST_LIFE   1.1f
#define BURST_SPEED  0.9f

typedef struct { float start, radius; } Burst;

static Burst bursts[MAX_BURSTS];
static int burstCount = 0;
static float clock_s = 0, hover = 0, mouseX = 0, mouseY = 0, lastSt = 1.0e9f;

static float sstep(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float wrap_pi(float d) {
    d = fmodf(d, TAU_F);
    if (d > PI) d -= TAU_F;
    if (d < -PI) d += TAU_F;
    return d;
}

// Blend from #A855F7 to #6366F1
static Color ring_color(float k) {
    return (Color){ (unsigned char)(168.0f + (99.0f - 168.0f) * k),
                    (unsigned char)(85.0f + (102.0f - 85.0f) * k),
                    (unsigned char)(247.0f + (241.0f - 247.0f) * k), 255 };
}

static void slice(Vector2 c, float r, float w, float startDeg, float endDeg, Color col, float alpha) {
    if (alpha < 0.004f) return;
    DrawRing(c, fmaxf(0.0f, r - w * 0.5f), r + w * 0.5f, startDeg, endDeg, 1, Fade(col, clampf(alpha, 0.0f, 1.0f)));
}

// One ring is a circle cut into slices. The brightness of each slice depends on how far
// it sits from the ring's direction, which is what makes the arcs taper off.
static void draw_arc(Vector2 center, float radius, float life, float cut, float angle,
                     float shiftX, float shiftY, float tmix, float widthMul, Vector2 cursor) {
    if (life <= 0.002f) return;

    float gain = 10.0f * radius * cut * cut * cut;
    float limit = gain > 6.2f ? cbrtf(6.2f / gain) : 2.0f;
    Color col = ring_color(tmix);
    Vector2 c = { center.x + shiftX * UNIT, center.y - shiftY * UNIT };
    float rpx = radius * UNIT;

    for (int k = 0; k < SEGMENTS; k++) {
        float th0 = TAU_F * k / SEGMENTS;
        float th1 = TAU_F * (k + 1) / SEGMENTS;
        float thm = (th0 + th1) * 0.5f;

        float a = fabsf(wrap_pi(thm - angle)) / PI;   // 0 at the ring direction, 1 on the far side
        if (a > limit) continue;
        float energy = life * fmaxf(expf(-gain * a * a * a) - 0.002f, 0.0f);
        if (energy < 0.01f) continue;

        // Brighter and thicker near the cursor
        float dx = shiftX + cosf(thm) * radius - cursor.x;
        float dy = shiftY + sinf(thm) * radius - cursor.y;
        float nearv = hover * HOVER_POWER * expf(-(dx * dx + dy * dy) / 0.03f);
        energy *= 1.0f + 1.6f * nearv;

        float w = LINE_W * (1.0f + 0.45f * nearv) * fmaxf(1.0f - a, 0.5f) * widthMul;
        float s = -th1 * RAD2DEG, e = -th0 * RAD2DEG;   // plane angles run counterclockwise

        slice(c, rpx, w * 7.0f, s, e, col, energy * GLOW * 0.07f);   // wide halo
        slice(c, rpx, w * 3.0f, s, e, col, energy * GLOW * 0.16f);   // tight halo
        slice(c, rpx, w,        s, e, col, energy * 1.2f);           // core line
        slice(c, rpx, w * 0.35f, s, e, WHITE, energy * 0.3f);        // hot center
    }
}

static float pulse_life(float t) {
    return fminf(sstep(0.0f, 0.7f, t), 1.0f - sstep(0.5f, 3.25f, t));
}

// The face parameters are not used by the rings, but main.c still eases them.
static Face rings_target(float t) {
    (void)t;
    return face_default();
}

static void rings_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)f; (void)t; (void)blink;

    // The state timer restarts whenever this animation is selected
    if (st < lastSt) {
        burstCount = 0; clock_s = 0; hover = 0; mouseX = 0; mouseY = 0;
    }
    lastSt = st;

    float dt = fminf(GetFrameTime(), 0.05f);
    clock_s += dt;
    float time_s = st * SPEED;
    bool introLive = time_s < (RING_COUNT + 2) * STAGGER + CYCLE;

    Vector2 center = { ox + FRAME_W * 0.5f, oy + FRAME_H * 0.5f };
    Vector2 m = GetMousePosition();
    bool inFrame = m.x >= ox && m.x < ox + FRAME_W && m.y >= oy && m.y < oy + FRAME_H;

    // Hover and parallax follow the mouse smoothly
    float k = 1.0f - expf(-dt * 8.0f);
    hover += ((inFrame ? 1.0f : 0.0f) - hover) * k;
    mouseX += ((inFrame ? (m.x - center.x) / UNIT : 0.0f) - mouseX) * k;
    mouseY += ((inFrame ? (center.y - m.y) / UNIT : 0.0f) - mouseY) * k;
    Vector2 cursor = { (m.x - center.x) / UNIT, (center.y - m.y) / UNIT };

    // Click inside the frame sends a wave outward
    if (inFrame && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (burstCount == MAX_BURSTS) {
            for (int i = 1; i < MAX_BURSTS; i++) bursts[i - 1] = bursts[i];
            burstCount--;
        }
        bursts[burstCount++] = (Burst){ clock_s, fmaxf(0.04f, BASE_RADIUS * 0.45f) };
    }
    int kept = 0;
    for (int i = 0; i < burstCount; i++)
        if (clock_s - bursts[i].start <= BURST_LIFE) bursts[kept++] = bursts[i];
    burstCount = kept;

    float scale = 1.0f + (HOVER_SCALE - 1.0f) * hover;

    BeginBlendMode(BLEND_ADDITIVE);

    for (int i = 0; i < RING_COUNT; i++) {
        float local = time_s - i * STAGGER;
        float tt = fmodf(local, CYCLE);
        if (tt < 0) tt += CYCLE;

        float life = (introLive && local < 0) ? 0.0f : pulse_life(tt);
        float opening = (introLive && local < CYCLE) ? sstep(0.0f, 0.9f, local) : 1.0f;
        float radius = (BASE_RADIUS + i * RADIUS_STEP
                        + (1.0f - powf(1.0f - tt / CYCLE, 2.0f)) * EXPANSION) * scale;

        // A click wave pushes nearby rings outward and brightens them
        for (int b = 0; b < burstCount; b++) {
            float age = clock_s - bursts[b].start;
            float wave = (bursts[b].radius + age * BURST_SPEED) * scale;
            float fade = powf(fmaxf(0.0f, 1.0f - age / BURST_LIFE), 1.5f);
            float gap = (wave - radius) / 0.08f;
            radius += 0.04f * expf(-gap * gap) * fade;
            life = fminf(1.6f, life + 0.9f * expf(-gap * gap * 1.6f) * fade);
        }

        float cut = powf(TAPER, (float)i) / fmaxf(0.08f, opening);
        float angle = ROTATION + SPIN * time_s * 0.6f * ((i % 2) ? -1.0f : 1.0f) * (1.0f + i * 0.15f);
        float tmix = RING_COUNT > 1 ? (float)i / (RING_COUNT - 1) : 0.0f;

        draw_arc(center, radius, life, cut, angle,
                 i * PARALLAX * mouseX, i * PARALLAX * mouseY, tmix, 1.0f, cursor);
    }

    // The wave itself, drawn as one extra ring per click
    for (int b = 0; b < burstCount; b++) {
        float age = clock_s - bursts[b].start;
        float fade = powf(fmaxf(0.0f, 1.0f - age / BURST_LIFE), 1.5f);
        float radius = (bursts[b].radius + age * BURST_SPEED) * scale;
        draw_arc(center, radius, 1.5f * fade, 0.85f, ROTATION, 0, 0,
                 clampf(age / BURST_LIFE, 0.0f, 1.0f), 1.15f, cursor);
    }

    EndBlendMode();
}

const Animation anim_rings = { "Rings", rings_target, rings_draw };
