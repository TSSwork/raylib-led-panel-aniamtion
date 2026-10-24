#include "raylib.h"
#include <math.h>
#include <stdlib.h>

#define SIDEBAR_W 200
#define FRAME_W   800
#define FRAME_H   600
#define WIN_W     (SIDEBAR_W + FRAME_W)
#define WIN_H     FRAME_H

#define BTN_H     56
#define BTN_GAP   10
#define BTN_PAD   12

typedef enum {
    ST_IDLE, ST_LISTENING, ST_THINKING, ST_SPEAKING,
    ST_HAPPY, ST_ERROR, ST_SLEEP, ST_COUNT
} State;

static const char *STATE_NAMES[ST_COUNT] = {
    "Idle", "Listening", "Thinking", "Speaking", "Happy", "Error", "Sleep"
};

// All fields are floats so the whole face can be eased smoothly between states.
typedef struct {
    float eyeW, eyeH, gap;
    float lookX, lookY;
    float mouthW, mouthCurve, mouthOpen;
    float r, g, b;
} Face;

static Face TargetFor(State s) {
    Face f = { 130, 150, 300,  0, 0,  120, 0.25f, 0,  90, 220, 255 };
    switch (s) {
        case ST_IDLE: break;
        case ST_LISTENING:
            f.eyeW = 145; f.eyeH = 185; f.mouthW = 60; f.mouthCurve = 0;
            f.r = 120; f.g = 255; f.b = 170; break;
        case ST_THINKING:
            f.lookX = -60; f.lookY = -55; f.eyeH = 110; f.mouthW = 50; f.mouthCurve = 0;
            f.r = 255; f.g = 200; f.b = 90; break;
        case ST_SPEAKING:
            f.mouthW = 170; f.mouthCurve = 0.1f; break;
        case ST_HAPPY:
            f.mouthW = 190; f.mouthCurve = 0.9f;
            f.r = 255; f.g = 150; f.b = 200; break;
        case ST_ERROR:
            f.eyeW = 150; f.eyeH = 70; f.mouthW = 150; f.mouthCurve = -0.6f;
            f.r = 255; f.g = 80; f.b = 80; break;
        case ST_SLEEP:
            f.eyeH = 10; f.mouthW = 0;
            f.r = 80; f.g = 100; f.b = 140; break;
        default: break;
    }
    return f;
}

static void EaseFace(Face *cur, const Face *tgt, float dt) {
    float k = 1.0f - expf(-9.0f * dt);
    float *c = (float *)cur;
    const float *t = (const float *)tgt;
    for (int i = 0; i < (int)(sizeof(Face) / sizeof(float)); i++)
        c[i] += (t[i] - c[i]) * k;
}

static void DrawMouth(float cx, float cy, const Face *f, Color col) {
    if (f->mouthW < 4) return;
    float half = f->mouthW * 0.5f;
    if (f->mouthOpen > 0.05f) {
        DrawEllipse((int)cx, (int)cy, half * 0.6f, 6 + f->mouthOpen * 40, col);
        return;
    }
    Vector2 prev = { 0 };
    for (int i = 0; i <= 20; i++) {
        float u = i / 10.0f - 1.0f;
        Vector2 p = { cx + u * half, cy + f->mouthCurve * 45.0f * (1.0f - u * u) };
        if (i > 0) DrawLineEx(prev, p, 8, col);
        prev = p;
    }
}

// Everything is drawn in 800x600 space, offset by (ox, oy).
static void DrawScene(State s, const Face *f, float t, float st, float blink, int ox, int oy) {
    float cx = ox + FRAME_W * 0.5f;
    float cy = oy + FRAME_H * 0.5f;
    Color col = { (unsigned char)f->r, (unsigned char)f->g, (unsigned char)f->b, 255 };

    float shake = 0;
    if (s == ST_ERROR) shake = sinf(st * 55.0f) * 12.0f * fmaxf(0.0f, 1.0f - st * 0.8f);

    if (s == ST_LISTENING) {
        for (int k = 0; k < 3; k++) {
            float p = fmodf(t * 0.7f + k / 3.0f, 1.0f);
            DrawRing((Vector2){ cx, cy }, 250 + p * 200, 256 + p * 200, 0, 360, 96, Fade(col, 1.0f - p));
        }
    }

    float ex = cx + shake + f->lookX;
    float ey = cy - 40 + f->lookY;
    float eh = f->eyeH * blink;
    for (int i = 0; i < 2; i++) {
        float x = ex + (i == 0 ? -f->gap * 0.5f : f->gap * 0.5f);
        if (s == ST_HAPPY) {
            DrawRing((Vector2){ x, ey + 30 }, 45, 75, 190, 350, 32, col);
        } else {
            Rectangle r = { x - f->eyeW * 0.5f, ey - eh * 0.5f, f->eyeW, eh };
            DrawRectangleRounded(r, 0.5f, 12, col);
        }
    }

    DrawMouth(cx + shake, cy + 150, f, col);

    if (s == ST_THINKING) {
        for (int i = 0; i < 3; i++) {
            float b = fmaxf(0.0f, sinf(t * 6.0f - i * 0.9f));
            DrawCircle((int)(cx + 190 + i * 36), (int)(cy - 190 - b * 18), 11, Fade(col, 0.5f + 0.5f * b));
        }
    }

    if (s == ST_SLEEP) {
        for (int i = 0; i < 3; i++) {
            float p = fmodf(t * 0.35f + i / 3.0f, 1.0f);
            DrawText("Z", (int)(cx + 150 + p * 90), (int)(cy - 80 - p * 160), (int)(24 + p * 30), Fade(col, 1.0f - p));
        }
    }
}

static Rectangle BtnRect(int i, float scroll) {
    return (Rectangle){ BTN_PAD, BTN_PAD + i * (BTN_H + BTN_GAP) - scroll,
                        SIDEBAR_W - 2 * BTN_PAD, BTN_H };
}

int main(void) {
    InitWindow(WIN_W, WIN_H, "Robot Face Animations");
    SetTargetFPS(60);

    State state = ST_IDLE;
    Face cur = TargetFor(ST_IDLE);
    float t = 0, stateT = 0, scroll = 0;
    float nextBlink = 2.0f, blinkStart = -1.0f;
    float nextGaze = 2.0f, gazeX = 0, gazeY = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;
        stateT += dt;

        // Sidebar input
        Vector2 m = GetMousePosition();
        float contentH = BTN_PAD + ST_COUNT * (BTN_H + BTN_GAP);
        float maxScroll = fmaxf(0.0f, contentH - WIN_H);
        if (m.x < SIDEBAR_W) scroll -= GetMouseWheelMove() * 40.0f;
        scroll = fminf(fmaxf(scroll, 0.0f), maxScroll);

        for (int i = 0; i < ST_COUNT; i++) {
            bool clicked = m.x < SIDEBAR_W && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
                           && CheckCollisionPointRec(m, BtnRect(i, scroll));
            if (clicked || IsKeyPressed(KEY_ONE + i)) { state = (State)i; stateT = 0; }
        }

        // Idle gaze wandering
        if (state == ST_IDLE && t > nextGaze) {
            gazeX = (float)GetRandomValue(-40, 40);
            gazeY = (float)GetRandomValue(-25, 25);
            nextGaze = t + 1.5f + GetRandomValue(0, 25) / 10.0f;
        }

        Face tgt = TargetFor(state);
        if (state == ST_IDLE) { tgt.lookX = gazeX; tgt.lookY = gazeY; }
        EaseFace(&cur, &tgt, dt);

        Face shown = cur;
        if (state == ST_SPEAKING) shown.mouthOpen = fmaxf(0.0f, sinf(t * 13.0f) * sinf(t * 4.1f));

        // Blinking
        if (t > nextBlink) { blinkStart = t; nextBlink = t + 2.0f + GetRandomValue(0, 30) / 10.0f; }
        float blink = 1.0f;
        float bt = t - blinkStart;
        if (bt >= 0 && bt < 0.16f) blink = fmaxf(0.06f, fabsf(bt / 0.08f - 1.0f));
        if (state == ST_SLEEP) blink = 1.0f;

        BeginDrawing();
        ClearBackground((Color){ 28, 30, 36, 255 });

        // Robot screen (800x600)
        BeginScissorMode(SIDEBAR_W, 0, FRAME_W, FRAME_H);
            DrawRectangle(SIDEBAR_W, 0, FRAME_W, FRAME_H, (Color){ 6, 8, 12, 255 });
            DrawScene(state, &shown, t, stateT, blink, SIDEBAR_W, 0);
        EndScissorMode();

        // Sidebar (200x600, scrollable)
        BeginScissorMode(0, 0, SIDEBAR_W, WIN_H);
            DrawRectangle(0, 0, SIDEBAR_W, WIN_H, (Color){ 36, 39, 48, 255 });
            for (int i = 0; i < ST_COUNT; i++) {
                Rectangle r = BtnRect(i, scroll);
                bool hover = m.x < SIDEBAR_W && CheckCollisionPointRec(m, r);
                Color bg = (i == (int)state) ? (Color){ 70, 110, 200, 255 }
                         : hover ? (Color){ 70, 74, 90, 255 } : (Color){ 52, 56, 68, 255 };
                DrawRectangleRounded(r, 0.25f, 8, bg);
                const char *label = TextFormat("%d  %s", i + 1, STATE_NAMES[i]);
                int w = MeasureText(label, 20);
                DrawText(label, (int)(r.x + (r.width - w) / 2), (int)(r.y + (r.height - 20) / 2), 20, RAYWHITE);
            }
            if (maxScroll > 0) {
                float barH = WIN_H * (float)WIN_H / contentH;
                float barY = scroll / maxScroll * (WIN_H - barH);
                DrawRectangle(SIDEBAR_W - 6, (int)barY, 4, (int)barH, GRAY);
            }
        EndScissorMode();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
