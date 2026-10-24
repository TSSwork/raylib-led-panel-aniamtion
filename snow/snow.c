#include "snow.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

#define TAU_F          (2.0f * PI)
#define SNOW_PIXEL     3        // screen pixels per snow pixel. Use 1 on the real LED panel
#define SNOW_VARIANT   0        // 0 = snowflake, 1 = square, 2 = round
#define DENSITY        0.5f     // 0 to 1, how many flakes
#define SPEED          1.0f     // fall speed multiplier
#define WIND           0.25f    // -2 to 2, sideways push
#define SWAY           0.5f     // how much flakes swing left/right
#define DEPTH          0.6f     // parallax: far flakes fall slower and dimmer
#define TWINKLE        0.35f    // sparkle strength
#define ACCUMULATE     1        // 1 = snow piles up at the bottom, 0 = no pile
#define INTRO_SECONDS  1.4f
#define MAX_FLAKES     160      // hard cap, lower this first if the ESP32 struggles
#define LEVELS         5
#define SLOTS          8
#define SNOW_GLOW      1

// Fall speeds in grid cells per second, and brightness per depth level
static const float LEVEL_FALL[LEVELS]  = { 5.0f, 8.0f, 11.0f, 16.0f, 22.0f };
static const float LEVEL_LIGHT[LEVELS] = { 0.42f, 0.58f, 0.76f, 0.92f, 1.0f };

// Sprite shapes, same as the original. '@' core, 'o' mid, 'x' edge, '.' empty
static const char *SPRITE_SRC[3][SLOTS][9] = {
    { // square
        { "@", 0 },
        { "oo", "oo", 0 },
        { "xox", "o@o", "xox", 0 },
        { "xoox", "o@@o", "o@@o", "xoox", 0 },
        { "xooox", "oo@oo", "o@@@o", "oo@oo", "xooox", 0 },
        { "xox", "o@o", "xox", 0 },
        { "xoox", "o@@o", "o@@o", "xoox", 0 },
        { "xooox", "oo@oo", "o@@@o", "oo@oo", "xooox", 0 }
    },
    { // round
        { "@", 0 },
        { ".x.", "x@x", ".x.", 0 },
        { ".xx.", "x@@x", "x@@x", ".xx.", 0 },
        { ".xxx.", "xo@ox", "x@@@x", "xo@ox", ".xxx.", 0 },
        { "..xxx..", ".xooox.", "xoo@oox", "xo@@@ox", "xoo@oox", ".xooox.", "..xxx..", 0 },
        { ".xx.", "x@@x", "x@@x", ".xx.", 0 },
        { ".xxx.", "xo@ox", "x@@@x", "xo@ox", ".xxx.", 0 },
        { "..xxx..", ".xooox.", "xoo@oox", "xo@@@ox", "xoo@oox", ".xooox.", "..xxx..", 0 }
    },
    { // snowflake
        { "@", 0 },
        { ".x.", "x@x", ".x.", 0 },
        { "..x..", "x.o.x", ".o@o.", "x.o.x", "..x..", 0 },
        { "...x...", ".x.o.x.", "..ooo..", "xoo@oox", "..ooo..", ".x.o.x.", "...x...", 0 },
        { "....x....", ".x..o..x.", "..o.o.o..", "...ooo...", "xooo@ooox",
          "...ooo...", "..o.o.o..", ".x..o..x.", 0 },
        { "x...x", ".o.o.", "..@..", ".o.o.", "x...x", 0 },
        { "x..x..x", ".o.o.o.", "..ooo..", "xoo@oox", "..ooo..", ".o.o.o.", "x..x..x", 0 },
        { "x...x...x", ".o..o..o.", "..o.o.o..", "...ooo...", "xooo@ooox",
          "...ooo...", "..o.o.o..", "x...x...x", 0 }
    }
};

typedef struct { int size; unsigned char px[81]; } Sprite;   // 0 = empty, else brightness
static Sprite sprites[3][SLOTS];
static bool spritesReady = false;

typedef struct {
    float x, y, vx, vy;
    int level;
    float phase, rate, drift, sparkle, reveal;
    bool alt;
} Flake;

static Flake flakes[MAX_FLAKES];
static int flakeCount = 0;
static float *pile = NULL;
static int gridW = 0, gridH = 0;
static float time_s = 0, introT = 0, lastSt = 1.0e9f;

static void build_sprites(void) {
    for (int v = 0; v < 3; v++)
        for (int s = 0; s < SLOTS; s++) {
            Sprite *sp = &sprites[v][s];
            int size = 0;
            while (SPRITE_SRC[v][s][size]) size++;
            sp->size = size;
            for (int i = 0; i < 81; i++) sp->px[i] = 0;
            for (int y = 0; y < size; y++)
                for (int x = 0; x < size; x++) {
                    char m = SPRITE_SRC[v][s][y][x];
                    sp->px[y * size + x] = (m == '@') ? 255 : (m == 'o') ? 165 : (m == 'x') ? 85 : 0;
                }
        }
    spritesReady = true;
}

static float rand01(void) { return (float)GetRandomValue(0, 10000) / 10000.0f; }

static int pick_level(void) {
    float r = rand01();
    if (r < 0.46f) return 0;
    if (r < 0.73f) return 1;
    if (r < 0.88f) return 2;
    if (r < 0.96f) return 3;
    return 4;
}

static void spawn_flake(Flake *f, bool top) {
    f->level = pick_level();
    f->x = rand01() * (gridW + 16) - 8;
    f->y = top ? -4.0f - rand01() * 10.0f : rand01() * gridH;
    f->vx = 0.0f;
    f->vy = LEVEL_FALL[f->level];
    f->phase = rand01() * TAU_F;
    f->rate = 0.6f + rand01() * 0.9f;
    f->sparkle = rand01() * 100.0f;
    f->drift = 0.7f + rand01() * 0.6f;
    f->alt = GetRandomValue(0, 1) == 1;
    f->reveal = rand01();
}

static void init_snow(void) {
    gridW = FRAME_W / SNOW_PIXEL; if (gridW < 1) gridW = 1;
    gridH = FRAME_H / SNOW_PIXEL; if (gridH < 1) gridH = 1;

    if (pile) free(pile);
    pile = (float *)calloc(gridW, sizeof(float));

    flakeCount = (int)(gridW * gridH * DENSITY / 90.0f);
    if (flakeCount < 8) flakeCount = 8;
    if (flakeCount > MAX_FLAKES) flakeCount = MAX_FLAKES;
    for (int i = 0; i < flakeCount; i++) spawn_flake(&flakes[i], false);

    if (!spritesReady) build_sprites();
    time_s = 0.0f;
    introT = 0.0f;
}

static float hash3(int a, int b, int c) {
    unsigned int h = (unsigned int)a * 374761393u + (unsigned int)b * 668265263u + (unsigned int)c * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFF) / 65536.0f;
}

static void simulate(float dt) {
    float windSpeed = WIND * 15.0f;
    for (int i = 0; i < flakeCount; i++) {
        Flake *f = &flakes[i];
        int level = f->level;
        float parallax = 1.0f - DEPTH * (1.0f - (level + 1) / (float)LEVELS) * 0.75f;
        float swing = sinf(time_s * f->rate + f->phase) * SWAY * (2.0f + level * 2.3f);
        float tx = (windSpeed * parallax + swing) * f->drift;
        float ty = LEVEL_FALL[level] * parallax * SPEED * f->drift;
        float ease = 1.0f - expf(-dt * 2.2f);
        f->vx += (tx - f->vx) * ease;
        f->vy += (ty - f->vy) * ease;
        f->x += f->vx * dt;
        f->y += f->vy * dt;

        if (ACCUMULATE && pile) {
            int col = (int)floorf(f->x);
            if (col >= 0 && col < gridW) {
                float surface = gridH - pile[col];
                if (f->y >= surface && pile[col] > 0.5f) {
                    int reach = level;
                    for (int k = -reach; k <= reach; k++) {
                        int idx = col + k;
                        if (idx >= 0 && idx < gridW)
                            pile[idx] += (0.35f + level * 0.15f) * (1.0f - fabsf((float)k) / (reach + 1));
                    }
                    spawn_flake(f, true);
                    continue;
                }
            }
        }
        if (f->y > gridH + 8) { spawn_flake(f, true); continue; }
        if (f->x < -8) f->x += gridW + 16;
        else if (f->x > gridW + 8) f->x -= gridW + 16;
    }
}

static void settle_pile(float dt) {
    if (!ACCUMULATE || !pile) return;
    float cap = fmaxf(4.0f, gridH * 0.08f);

    // slow dune-shaped growth so the pile looks natural
    float grow = (0.25f + DENSITY * 0.9f) * dt * fminf(1.0f, SPEED);
    for (int i = 0; i < gridW; i++) {
        float dune = 0.55f + 0.45f * sinf(i * 0.045f + sinf(i * 0.013f) * 2.1f) * sinf(i * 0.021f + 1.3f);
        if (pile[i] < cap * dune) pile[i] += grow * (0.6f + 0.8f * dune);
    }
    // avalanche smoothing, two passes
    for (int pass = 0; pass < 2; pass++) {
        for (int k = 0; k < gridW - 1; k++) {
            int i = (pass == 0) ? k : gridW - 2 - k;
            float diff = pile[i] - pile[i + 1];
            if (fabsf(diff) > 1.2f) {
                float move = (fabsf(diff) - 1.2f) * 0.5f * (diff > 0 ? 1.0f : -1.0f);
                pile[i] -= move;
                pile[i + 1] += move;
            }
        }
    }
    // melt anything over the cap
    for (int i = 0; i < gridW; i++) {
        float excess = fmaxf(0.0f, pile[i] - cap);
        pile[i] = fmaxf(0.0f, pile[i] - excess * 0.8f * dt);
    }
}

static Face snow_target(float t) {
    (void)t;
    return face_default();
}

static void snow_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)f; (void)t; (void)blink;

    if (st < lastSt) init_snow();
    lastSt = st;
    if (!pile) init_snow();

    float dt = fminf(GetFrameTime(), 0.05f);
    time_s += dt;
    if (introT < 1.0f) introT = fminf(1.0f, introT + dt / INTRO_SECONDS);

    simulate(dt);
    settle_pile(dt);

    Color base = { 230, 240, 255, 255 };   // slightly cool white reads nicer on black

    BeginBlendMode(BLEND_ADDITIVE);

    if (ACCUMULATE && pile) {
        int tick = (int)(time_s * 1.1f);
        for (int col = 0; col < gridW; col++) {
            int rows = (int)floorf(pile[col] + 0.5f);
            for (int d = 1; d <= rows; d++) {
                int y = gridH - d;
                if (y < 0) break;
                float b = (d == 1) ? 1.0f : fmaxf(0.5f, 0.94f - (d - 1) * 0.13f);
                if (d <= 3 && hash3(col, d, tick) > 0.992f) b = 1.0f;
                DrawRectangle(ox + col * SNOW_PIXEL, oy + y * SNOW_PIXEL,
                              SNOW_PIXEL, SNOW_PIXEL, Fade(base, b * 0.9f));
            }
        }
    }

    for (int i = 0; i < flakeCount; i++) {
        Flake *fl = &flakes[i];
        int level = fl->level;

        float reveal = 1.0f;
        if (introT < 1.0f) {
            float sweep = introT * 1.75f;
            reveal = clampf((sweep - fl->y / gridH - fl->reveal * 0.25f) / 0.35f, 0.0f, 1.0f);
        }
        if (reveal <= 0.0f) continue;

        float s = sinf(time_s * 0.9f + fl->sparkle); if (s < 0.0f) s = 0.0f;
        float s2 = s * s, s4 = s2 * s2, s8 = s4 * s4, s16 = s8 * s8;
        float spark = s16 * TWINKLE;

        float depthLight = 1.0f - DEPTH * (1.0f - LEVEL_LIGHT[level]);
        float light = fminf(1.0f, depthLight * (0.85f + spark * 0.6f)) * reveal * reveal;

        int slot = (level >= 2 && fl->alt) ? level + 3 : level;
        Sprite *sp = &sprites[SNOW_VARIANT][slot];
        int half = sp->size / 2;
        int bx = (int)floorf(fl->x) - half;
        int by = (int)floorf(fl->y) - half;
        if (by + sp->size <= 0 || by >= gridH) continue;

        #if SNOW_GLOW
                Vector2 c = { ox + (bx + sp->size * 0.5f) * SNOW_PIXEL,
                              oy + (by + sp->size * 0.5f) * SNOW_PIXEL };
                float radius = sp->size * SNOW_PIXEL * 0.9f;
                DrawCircleV(c, radius,        Fade(base, 0.05f * light));
                DrawCircleV(c, radius * 0.5f, Fade(base, 0.09f * light));
        #endif

        for (int py = 0; py < sp->size; py++) {
            int sy = by + py;
            if (sy < 0 || sy >= gridH) continue;
            for (int px = 0; px < sp->size; px++) {
                unsigned char mul = sp->px[py * sp->size + px];
                if (!mul) continue;
                int sx = bx + px;
                if (sx < 0 || sx >= gridW) continue;
                float a = light * mul / 255.0f;
                Color tint = (mul == 255) ? WHITE : base;   // hot core, cooler arms
                DrawRectangle(ox + sx * SNOW_PIXEL, oy + sy * SNOW_PIXEL,
                              SNOW_PIXEL, SNOW_PIXEL, Fade(tint, a));
            }
        }
    }

    EndBlendMode();
}

const Animation anim_snow = { "Snow", snow_target, snow_draw };
