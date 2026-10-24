#include "ascii.h"
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

#define CELL_SIZE      12       // pixels per cell
#define INTRO_SECONDS  1.8f
#define REST_RATE      0.05f    // ambient character change rate
#define GLITCH_RATE    0.02f    // glitch ignition rate

static const Color BASE_COLOR = { 80, 80, 90, 255 };
static const Color GLITCH_1   = { 97, 220, 163, 255 };   // #61dca3
static const Color GLITCH_2   = { 168, 85, 247, 255 };   // #A855F7

typedef struct {
    char glyph;
    float heat;
    float peak;
    float hold_time;
    float fade_time;
    float delay;      // countdown before ignition, used to sweep runs
    int color_idx;
} Cell;

static Cell *cells = NULL;
static float *delays = NULL;
static int grid_cols = 0;
static int grid_rows = 0;
static int total_cells = 0;
static float intro_timer = 0.0f;
static float lastSt = 1.0e9f;
static float rest_carry = 0.0f;
static float glitch_carry = 0.0f;
static Font default_font;
static bool font_loaded = false;

static void init_grid(void) {
    grid_cols = FRAME_W / CELL_SIZE;
    grid_rows = FRAME_H / CELL_SIZE;
    if (grid_cols < 1) grid_cols = 1;
    if (grid_rows < 1) grid_rows = 1;
    total_cells = grid_cols * grid_rows;

    if (cells) free(cells);
    if (delays) free(delays);

    cells = (Cell *)malloc(total_cells * sizeof(Cell));
    delays = (float *)malloc(total_cells * sizeof(float));

    for (int r = 0; r < grid_rows; r++) {
        for (int c = 0; c < grid_cols; c++) {
            int i = r * grid_cols + c;
            cells[i].glyph = (char)GetRandomValue(33, 126);
            cells[i].heat = 0.0f;
            cells[i].peak = 0.0f;
            cells[i].hold_time = 0.0f;
            cells[i].fade_time = 0.5f;
            cells[i].delay = 0.0f;
            cells[i].color_idx = 0;

            float dx = (c - grid_cols * 0.5f) / grid_cols;
            float dy = (r - grid_rows * 0.5f) / grid_rows;
            float dist = sqrtf(dx*dx + dy*dy);
            delays[i] = dist * INTRO_SECONDS * 1.2f + GetRandomValue(0, 40) / 100.0f;
        }
    }

    if (!font_loaded) {
        default_font = GetFontDefault();
        font_loaded = true;
    }
}

// Light one cell. "after" delays the ignition so a run sweeps across.
static void ignite(int i, float strength, float holdTime, float fadeTime, int colorIdx, float after) {
    Cell *cell = &cells[i];
    if (cell->delay > 0.0f || cell->heat > 0.01f) {
        // already queued or lit: keep the stronger / longer of the two
        if (cell->heat < 0.15f && cell->delay <= 0.0f) cell->color_idx = colorIdx;
        cell->peak = fmaxf(cell->peak, strength);
        cell->hold_time = fmaxf(cell->hold_time, holdTime);
        cell->fade_time = fmaxf(cell->fade_time, fadeTime);
        cell->delay = fminf(cell->delay, after);
        return;
    }
    cell->peak = strength;
    cell->hold_time = holdTime;
    cell->fade_time = fadeTime;
    cell->color_idx = colorIdx;
    cell->delay = after;
    if (after <= 0.0f) cell->heat = strength;
}

static Face ascii_target(float t) {
    (void)t;
    return face_default();
}

static void ascii_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)f; (void)t; (void)blink;

    if (st < lastSt) {
        if (cells) { free(cells); cells = NULL; }
        if (delays) { free(delays); delays = NULL; }
        intro_timer = 0.0f;
        rest_carry = 0.0f;
        glitch_carry = 0.0f;
    }
    lastSt = st;

    if (!cells) init_grid();

    float dt = fminf(GetFrameTime(), 0.05f);
    intro_timer += dt;

    int base_x = ox + (FRAME_W - grid_cols * CELL_SIZE) / 2;
    int base_y = oy + (FRAME_H - grid_rows * CELL_SIZE) / 2;

    // 1. Ambient character changes (batched)
    rest_carry += total_cells * REST_RATE * dt;
    int rest_changes = (int)rest_carry;
    rest_carry -= rest_changes;
    for (int k = 0; k < rest_changes; k++) {
        int i = GetRandomValue(0, total_cells - 1);
        if (cells[i].heat < 0.1f && cells[i].delay <= 0.0f)
            cells[i].glyph = (char)GetRandomValue(33, 126);
    }

    // 2. Glitch events: single letters, row runs, column runs, long sweeps
    glitch_carry += total_cells * GLITCH_RATE * dt;
    int glitch_changes = (int)glitch_carry;
    glitch_carry -= glitch_changes;
    while (glitch_changes-- > 0) {
        int i = GetRandomValue(0, total_cells - 1);
        int colorIdx = GetRandomValue(1, 2);
        float strength = 0.45f + 0.55f * ((float)GetRandomValue(0, 100) / 100.0f);
        float holdTime = 0.04f + 0.26f * ((float)GetRandomValue(0, 100) / 100.0f);
        float fadeTime = 0.45f + 0.6f * ((float)GetRandomValue(0, 100) / 100.0f);
        int roll = GetRandomValue(0, 99);

        if (roll < 2) {
            // rare long sweep across the rest of a row
            int c = i % grid_cols;
            int len = 10 + GetRandomValue(0, 24);
            if (len > grid_cols - c) len = grid_cols - c;
            for (int k = 0; k < len; k++)
                ignite(i + k, strength, holdTime, fadeTime, colorIdx, k * 0.012f);
        } else if (roll < 14) {
            // short horizontal run (2 to 8 letters)
            int c = i % grid_cols;
            int len = 2 + GetRandomValue(0, 6);
            if (len > grid_cols - c) len = grid_cols - c;
            for (int k = 0; k < len; k++)
                ignite(i + k, strength, holdTime, fadeTime, colorIdx, k * 0.028f);
        } else if (roll < 26) {
            // short vertical run (2 to 8 letters)
            int r = i / grid_cols;
            int len = 2 + GetRandomValue(0, 6);
            if (len > grid_rows - r) len = grid_rows - r;
            for (int k = 0; k < len; k++)
                ignite(i + k * grid_cols, strength, holdTime, fadeTime, colorIdx, k * 0.028f);
        } else {
            // single letter
            ignite(i, strength, holdTime, fadeTime, colorIdx, 0.0f);
        }
    }

    // 3. Heat decay and hot-letter flicker
    for (int i = 0; i < total_cells; i++) {
        Cell *cell = &cells[i];
        if (cell->delay > 0.0f) {
            cell->delay -= dt;
            if (cell->delay <= 0.0f) {
                cell->delay = 0.0f;
                cell->heat = cell->peak;
                cell->glyph = (char)GetRandomValue(33, 126);
            }
            continue;
        }
        if (cell->heat > 0.0f) {
            if (cell->hold_time > 0.0f) {
                cell->hold_time -= dt;
                cell->heat += (cell->peak - cell->heat) * fminf(1.0f, dt / 0.05f);
            } else {
                cell->heat -= dt / fmaxf(cell->fade_time, 0.05f);
                if (cell->heat < 0.0f) cell->heat = 0.0f;
            }
            // while hot, the letter flickers through random glyphs
            if (cell->heat > 0.1f && (float)GetRandomValue(0, 9999) / 10000.0f < 15.0f * dt)
                cell->glyph = (char)GetRandomValue(33, 126);
        }
    }

    // 4. Draw
    float font_size = (float)CELL_SIZE - 2.0f;
    float spacing = 0.0f;

    for (int r = 0; r < grid_rows; r++) {
        for (int c = 0; c < grid_cols; c++) {
            int i = r * grid_cols + c;
            if (intro_timer < delays[i]) continue;

            Cell *cell = &cells[i];
            char str[2] = { cell->glyph, '\0' };
            Color color;
            float alpha;

            if (cell->heat > 0.01f) {
                color = (cell->color_idx == 1) ? GLITCH_1 : GLITCH_2;
                alpha = cell->heat;
            } else {
                color = BASE_COLOR;
                alpha = 0.2f;
            }

            float dx = (c - grid_cols * 0.5f) / grid_cols;
            float dy = (r - grid_rows * 0.5f) / grid_rows;
            float vignette = 1.0f - (dx*dx + dy*dy) * 0.8f;
            alpha *= fmaxf(0.1f, vignette);

            int shake_x = 0, shake_y = 0;
            if (cell->heat > 0.5f) {
                shake_x = GetRandomValue(-1, 1);
                shake_y = GetRandomValue(-1, 1);
            }

            Vector2 pos = {
                (float)(base_x + c * CELL_SIZE + shake_x),
                (float)(base_y + r * CELL_SIZE + shake_y + 2)
            };

            DrawTextEx(default_font, str, pos, font_size, spacing, Fade(color, alpha));
        }
    }
}

const Animation anim_ascii = { "ASCII", ascii_target, ascii_draw };
