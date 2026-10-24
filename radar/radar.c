#include "radar.h"
#include <math.h>
#include <stdbool.h>

#define TAU_F         (2.0f * PI)
#define RING_COUNT    6
#define SPOKE_COUNT   12
#define TARGETS       6
#define HISTORY       4
#define MAX_SPECKS    260      // clutter specks per set, lower this for a lighter radar
#define INTRO_SECONDS 2.2f
#define SWEEP_PERIOD  6.0f     // seconds for one full turn
#define TRAIL         0.35f    // 0.02 to 1, higher means a longer tail
#define RADAR_R       270.0f   // radar radius in pixels inside the 800x600 frame
#define PX            1.5f     // size scale for blips and pings

static const Color ACCENT      = { 62, 240, 154, 255 };    // #3ef09a
static const Color ACCENT_EDGE = { 120, 245, 184, 255 };

typedef struct { float x, y, time; } Echo;
typedef struct {
    float x, y, heading, velocity, turn;
    Echo echoes[HISTORY];
    int echoCount;
} Contact;
typedef struct { float x, y, bearing, power; } Speck;

static Contact contacts[TARGETS];
static Contact cursor;
static bool cursorInside = false;
static Speck specks[2][MAX_SPECKS];     // two sets, so each pass keeps its own clutter
static float time_s = 0, sweep = 0, swept = 0;
static int cycle = 0;
static float lastSt = 1.0e9f;

// Positions use plane units (radius 1 is the outer ring), x to the east, y to the north.
// Bearings are radians from north, clockwise.

static float sstep(float a, float b, float x) {
    float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static float to_deg(float bearing) {
    return bearing * RAD2DEG - 90.0f;   // raylib angles start at east
}

static Vector2 polar(Vector2 c, float bearing, float r) {
    return (Vector2){ c.x + sinf(bearing) * r * RADAR_R, c.y - cosf(bearing) * r * RADAR_R };
}

static void ring_arc(Vector2 c, float r, float w, float b0, float b1, Color col) {
    float radius = r * RADAR_R;
    DrawRing(c, radius - w * 0.5f, radius + w * 0.5f, to_deg(b0), to_deg(b1), 0, col);
}

static float trail_at(float behind, float trailW) {
    return expf(-behind / trailW) * (1.0f - sstep(swept - 0.15f, swept, behind));
}

static void spawn_contact(Contact *c, bool inward) {
    float angle = randf(0.0f, TAU_F);
    float radius = inward ? 0.94f : randf(0.2f, 0.88f);
    c->x = sinf(angle) * radius;
    c->y = cosf(angle) * radius;
    c->heading = inward ? angle + PI + randf(-0.8f, 0.8f) : randf(0.0f, TAU_F);
    c->velocity = randf(0.012f, 0.042f);
    c->turn = randf(-0.09f, 0.09f);
}

static void reseed_specks(int set) {
    Vector2 centers[8];
    for (int i = 0; i < 8; i++) {
        float a = randf(0.0f, TAU_F), r = randf(0.15f, 0.8f);
        centers[i] = (Vector2){ sinf(a) * r, cosf(a) * r };
    }
    for (int i = 0; i < MAX_SPECKS; i++) {
        float x, y;
        if (GetRandomValue(0, 99) < 65) {          // most specks gather in weather clusters
            Vector2 cc = centers[GetRandomValue(0, 7)];
            x = cc.x + randf(-0.12f, 0.12f);
            y = cc.y + randf(-0.12f, 0.12f);
        } else {
            float a = randf(0.0f, TAU_F), r = sqrtf(randf(0.0f, 1.0f)) * 0.85f;
            x = sinf(a) * r;
            y = cosf(a) * r;
        }
        float b = atan2f(x, y);
        if (b < 0) b += TAU_F;
        Speck *k = &specks[set][i];
        k->x = x; k->y = y; k->bearing = b;
        k->power = hypotf(x, y) > 0.95f ? 0.0f : randf(0.35f, 1.0f);
    }
}

static void reset(void) {
    time_s = 0; sweep = 0; swept = 0; cycle = 0;
    for (int i = 0; i < TARGETS; i++) {
        spawn_contact(&contacts[i], false);
        contacts[i].echoCount = 0;
    }
    cursor.echoCount = 0;
    cursorInside = false;
    reseed_specks(0);
    reseed_specks(1);
}

static void record_echo(Contact *c) {
    int n = c->echoCount < HISTORY ? c->echoCount : HISTORY - 1;
    for (int i = n; i > 0; i--) c->echoes[i] = c->echoes[i - 1];
    c->echoes[0] = (Echo){ c->x, c->y, time_s };
    if (c->echoCount < HISTORY) c->echoCount++;
}

// True when the sweep line crossed this position during the last step.
static bool passed(float x, float y, float previous, float delta) {
    float gap = fmodf(atan2f(x, y) - previous, TAU_F);
    if (gap < 0) gap += TAU_F;
    return gap > 0 && gap <= delta;
}

static void advance(float dt) {
    float delta = dt / SWEEP_PERIOD * TAU_F;
    float previous = sweep;
    swept += delta;

    for (int i = 0; i < TARGETS; i++) {
        Contact *ct = &contacts[i];
        ct->heading += ct->turn * dt;
        ct->x += sinf(ct->heading) * ct->velocity * dt;
        ct->y += cosf(ct->heading) * ct->velocity * dt;
        if (hypotf(ct->x, ct->y) > 0.96f) spawn_contact(ct, true);
        if (passed(ct->x, ct->y, previous, delta)) record_echo(ct);
    }
    if (cursorInside && passed(cursor.x, cursor.y, previous, delta)) record_echo(&cursor);

    sweep += delta;
    while (sweep >= TAU_F) {
        sweep -= TAU_F;
        cycle++;
        reseed_specks(cycle & 1);   // fresh clutter for the pass that just started
    }
}

static void draw_contact(const Contact *ct, float strength, Vector2 c, float spriteIn) {
    float persist = SWEEP_PERIOD * 1.1f;
    for (int i = 0; i < ct->echoCount; i++) {
        const Echo *e = &ct->echoes[i];
        float age = time_s - e->time;
        float fade = expf(-age / persist) * (i == 0 ? 1.0f : 0.8f);
        if (fade < 0.01f) continue;

        Vector2 p = { c.x + e->x * RADAR_R, c.y - e->y * RADAR_R };
        float a = clampf(fade * strength * spriteIn, 0.0f, 1.0f);
        float rad = PX * (i == 0 ? 3.4f : 2.4f);
        DrawCircleV(p, rad, Fade(ACCENT, a * 0.9f));
        if (i == 0) DrawCircleV(p, rad * 0.45f, Fade(WHITE, a * 0.6f));

        if (i == 0 && age < 1.4f) {                 // expanding ping ring
            float prog = age / 1.4f;
            float eased = 1.0f - powf(1.0f - prog, 3.0f);
            float pr = PX * (4.0f + 26.0f * eased);
            float pa = (1.0f - prog) * (1.0f - prog) * strength * spriteIn;
            DrawRing(p, pr - 1.0f, pr + 1.0f, 0, 360, 0, Fade(ACCENT_EDGE, pa));
        }
    }
}

// The face parameters are not used by the radar, but main.c still eases them.
static Face radar_target(float t) {
    (void)t;
    return face_default();
}

static void radar_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)f; (void)t; (void)blink;

    // The state timer restarts whenever this animation is selected
    if (st < lastSt) reset();
    lastSt = st;

    float dt = fminf(GetFrameTime(), 0.05f);
    float intro = clampf(st / INTRO_SECONDS, 0.0f, 1.0f);
    float sweepIn = sstep(0.45f, 1.0f, intro);
    float ramp = 0.25f + 0.75f * clampf((intro - 0.35f) / 0.65f, 0.0f, 1.0f);
    float trailW = TRAIL * TAU_F * 0.5f;

    Vector2 c = { ox + FRAME_W * 0.5f, oy + FRAME_H * 0.5f };

    // The mouse acts as an extra contact that the sweep can ping
    Vector2 m = GetMousePosition();
    float mx = (m.x - c.x) / RADAR_R, my = (c.y - m.y) / RADAR_R;
    bool inFrame = m.x >= ox && m.x < ox + FRAME_W && m.y >= oy && m.y < oy + FRAME_H;
    cursorInside = inFrame && hypotf(mx, my) < 0.98f;
    if (cursorInside) { cursor.x = mx; cursor.y = my; }

    time_s += dt;
    advance(dt * ramp);

    // Static grid
    for (int i = 1; i < RING_COUNT; i++) {
        float arc = clampf(intro * (RING_COUNT + 3) - i, 0.0f, 1.0f);
        if (arc > 0) ring_arc(c, (float)i / RING_COUNT, 1.5f, 0, arc * TAU_F, Fade(WHITE, 0.16f));
    }
    float outerArc = clampf(intro * (RING_COUNT + 3) - RING_COUNT, 0.0f, 1.0f);
    if (outerArc > 0) ring_arc(c, 1.0f, 2.0f, 0, outerArc * TAU_F, Fade(WHITE, 0.32f));

    float spokesIn = sstep(0.35f, 0.8f, intro);
    if (spokesIn > 0) {
        for (int k = 0; k < SPOKE_COUNT; k++) {
            float b = k * TAU_F / SPOKE_COUNT;
            DrawLineEx(polar(c, b, 0.05f), polar(c, b, 1.0f), 1.0f, Fade(WHITE, 0.06f * spokesIn));
        }
        for (int k = 0; k < 4; k++) {
            float b = k * TAU_F / 4.0f;
            DrawLineEx(polar(c, b, 0.02f), polar(c, b, 1.0f), 1.5f, Fade(WHITE, 0.12f * spokesIn));
            for (int j = 1; j < RING_COUNT * 5; j++) {      // small range marks on the axes
                float r = (float)j / (RING_COUNT * 5);
                if (r < 0.1f) continue;
                float len = (j % 5 == 0) ? 5.0f : 3.0f;
                Vector2 p = polar(c, b, r);
                Vector2 d = { cosf(b) * len, sinf(b) * len };
                DrawLineV((Vector2){ p.x - d.x, p.y - d.y }, (Vector2){ p.x + d.x, p.y + d.y },
                          Fade(WHITE, 0.2f * spokesIn));
            }
        }
        ring_arc(c, 0.022f, 1.5f, 0, TAU_F, Fade(WHITE, 0.3f * spokesIn));
    }

    float ticksIn = sstep(0.55f, 0.9f, intro);
    if (ticksIn > 0) {
        for (int i = 0; i < 180; i++) {
            bool major = (i % 15 == 0), middle = (i % 5 == 0);
            float len = major ? 0.05f : (middle ? 0.03f : 0.016f);
            float a = major ? 0.36f : (middle ? 0.26f : 0.15f);
            float b = i * TAU_F / 180.0f;
            DrawLineEx(polar(c, b, 1.0f), polar(c, b, 1.0f + len), 1.5f, Fade(WHITE, a * ticksIn));
        }
        ring_arc(c, 1.065f, 1.5f, 0, TAU_F, Fade(WHITE, 0.10f * sstep(0.6f, 0.95f, intro)));
    }

    // Glowing layer, drawn additively
    BeginBlendMode(BLEND_ADDITIVE);

    if (sweepIn > 0) {
        float span = fminf(swept, TAU_F * 0.6f);

        // Trail fill behind the beam
        const int slices = 60;
        for (int i = 0; i < slices; i++) {
            float b0 = span * i / slices, b1 = span * (i + 1) / slices;
            float tr = trail_at((b0 + b1) * 0.5f, trailW) * sweepIn;
            if (tr < 0.01f) continue;
            DrawCircleSector(c, RADAR_R, to_deg(sweep - b1), to_deg(sweep - b0), 1, Fade(ACCENT, tr * 0.28f));
        }

        // Grid rings light up where the beam just passed
        const int chunks = 20;
        for (int i = 0; i < chunks; i++) {
            float b0 = span * i / chunks, b1 = span * (i + 1) / chunks;
            float glow = powf(trail_at((b0 + b1) * 0.5f, trailW) * sweepIn, 0.45f);
            if (glow < 0.02f) continue;
            for (int r = 1; r <= RING_COUNT; r++)
                ring_arc(c, (float)r / RING_COUNT, 2.0f, sweep - b1, sweep - b0, Fade(ACCENT, glow * 0.45f));
        }

        // Clutter specks. Each pass has its own set, like weather returns that drift.
        for (int s = 0; s < 2; s++) {
            bool current = (s == (cycle & 1));
            for (int i = 0; i < MAX_SPECKS; i++) {
                const Speck *k = &specks[s][i];
                if ((k->bearing <= sweep) != current) continue;
                float behind = sweep - k->bearing;
                if (behind < 0) behind += TAU_F;
                float tr = trail_at(behind, trailW) * sweepIn;
                if (tr < 0.02f || k->power <= 0) continue;
                DrawCircleV((Vector2){ c.x + k->x * RADAR_R, c.y - k->y * RADAR_R }, 1.8f,
                            Fade(ACCENT, tr * k->power * 0.8f));
            }
        }

        // The beam itself
        Vector2 tip = polar(c, sweep, 1.0f);
        DrawLineEx(c, tip, 8.0f, Fade(ACCENT, 0.18f * sweepIn));
        DrawLineEx(c, tip, 2.5f, Fade(ACCENT_EDGE, 0.95f * sweepIn));
    }

    float spriteIn = fminf(1.0f, intro * 2.0f);
    for (int i = 0; i < TARGETS; i++) draw_contact(&contacts[i], 1.0f, c, spriteIn);
    draw_contact(&cursor, 1.3f, c, spriteIn);

    EndBlendMode();
}

const Animation anim_radar = { "Radar", radar_target, radar_draw };
