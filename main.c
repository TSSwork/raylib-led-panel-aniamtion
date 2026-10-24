#include "raylib.h"
#include <math.h>

#include "common/common.h"
#include "face_idle/face_idle.h"
#include "face_listening/face_listening.h"
#include "face_thinking/face_thinking.h"
#include "face_speaking/face_speaking.h"
#include "face_happy/face_happy.h"
#include "face_error/face_error.h"
#include "face_sleep/face_sleep.h"
#include "matrix_thinking/matrix_thinking.h"
#include "fireworks/fireworks.h"
#include "radar/radar.h"

#define BTN_H   56
#define BTN_GAP 10
#define BTN_PAD 12

// The order here is the order of the sidebar buttons.
static const Animation *ANIMS[] = {
    &anim_idle, &anim_listening, &anim_thinking, &anim_speaking,
    &anim_happy, &anim_error, &anim_sleep, &anim_matrix_thinking,
    &anim_fireworks, &anim_radar,
};
#define ANIM_COUNT ((int)(sizeof(ANIMS) / sizeof(ANIMS[0])))

static Rectangle BtnRect(int i, float scroll) {
    return (Rectangle){ BTN_PAD, BTN_PAD + i * (BTN_H + BTN_GAP) - scroll,
                        SIDEBAR_W - 2 * BTN_PAD, BTN_H };
}

int main(void) {
    InitWindow(WIN_W, WIN_H, "Robot Face Animations");
    SetTargetFPS(60);

    int current = 0;
    Face cur = face_default();
    float t = 0, stateT = 0, scroll = 0;
    float nextBlink = 2.0f, blinkStart = -1.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;
        stateT += dt;

        // Sidebar input
        Vector2 m = GetMousePosition();
        float contentH = BTN_PAD + ANIM_COUNT * (BTN_H + BTN_GAP);
        float maxScroll = fmaxf(0.0f, contentH - WIN_H);
        if (m.x < SIDEBAR_W) scroll -= GetMouseWheelMove() * 40.0f;
        scroll = fminf(fmaxf(scroll, 0.0f), maxScroll);

        for (int i = 0; i < ANIM_COUNT; i++) {
            bool clicked = m.x < SIDEBAR_W && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)
                           && CheckCollisionPointRec(m, BtnRect(i, scroll));
            bool key = i < 9 && IsKeyPressed(KEY_ONE + i);
            if (clicked || key) { current = i; stateT = 0; }
        }

        // Ease toward the current animation's target face
        Face tgt = ANIMS[current]->target(t);
        face_ease(&cur, &tgt, dt);

        // Blinking
        if (t > nextBlink) { blinkStart = t; nextBlink = t + 2.0f + GetRandomValue(0, 30) / 10.0f; }
        float blink = 1.0f;
        float bt = t - blinkStart;
        if (bt >= 0 && bt < 0.16f) blink = fmaxf(0.06f, fabsf(bt / 0.08f - 1.0f));

        BeginDrawing();
        ClearBackground((Color){ 28, 30, 36, 255 });

        // Robot screen (800x600)
        BeginScissorMode(SIDEBAR_W, 0, FRAME_W, FRAME_H);
            DrawRectangle(SIDEBAR_W, 0, FRAME_W, FRAME_H, (Color){ 6, 8, 12, 255 });
            ANIMS[current]->draw(&cur, t, stateT, blink, SIDEBAR_W, 0);
        EndScissorMode();

        // Sidebar (200x600, scrollable)
        BeginScissorMode(0, 0, SIDEBAR_W, WIN_H);
            DrawRectangle(0, 0, SIDEBAR_W, WIN_H, (Color){ 36, 39, 48, 255 });
            for (int i = 0; i < ANIM_COUNT; i++) {
                Rectangle r = BtnRect(i, scroll);
                bool hover = m.x < SIDEBAR_W && CheckCollisionPointRec(m, r);
                Color bg = (i == current) ? (Color){ 70, 110, 200, 255 }
                         : hover ? (Color){ 70, 74, 90, 255 } : (Color){ 52, 56, 68, 255 };
                DrawRectangleRounded(r, 0.25f, 8, bg);
                const char *label = TextFormat("%d  %s", i + 1, ANIMS[i]->name);
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
