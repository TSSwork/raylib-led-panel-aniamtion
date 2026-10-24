#include "common.h"
#include <math.h>

Face face_default(void) {
    Face f = { 130, 150, 300,  0, 0,  120, 0.25f, 0,  90, 220, 255 };
    return f;
}

void face_ease(Face *cur, const Face *tgt, float dt) {
    float k = 1.0f - expf(-9.0f * dt);
    float *c = (float *)cur;
    const float *t = (const float *)tgt;
    for (int i = 0; i < (int)(sizeof(Face) / sizeof(float)); i++)
        c[i] += (t[i] - c[i]) * k;
}

Color face_color(const Face *f) {
    Color c = { (unsigned char)f->r, (unsigned char)f->g, (unsigned char)f->b, 255 };
    return c;
}

void draw_eyes(const Face *f, float blink, float shake, int ox, int oy) {
    float cx = ox + FRAME_W * 0.5f + shake + f->lookX;
    float cy = oy + FRAME_H * 0.5f - 40 + f->lookY;
    float eh = f->eyeH * blink;
    Color col = face_color(f);
    for (int i = 0; i < 2; i++) {
        float x = cx + (i == 0 ? -f->gap * 0.5f : f->gap * 0.5f);
        Rectangle r = { x - f->eyeW * 0.5f, cy - eh * 0.5f, f->eyeW, eh };
        DrawRectangleRounded(r, 0.5f, 12, col);
    }
}

void draw_mouth(const Face *f, float shake, int ox, int oy) {
    if (f->mouthW < 4) return;
    float cx = ox + FRAME_W * 0.5f + shake;
    float cy = oy + FRAME_H * 0.5f + 150;
    float half = f->mouthW * 0.5f;
    Color col = face_color(f);

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

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void draw_dot_matrix(int ox, int oy, float size, int dots, float scale,
                     float t, Color color, DotIntensityFn intensity) {
    int grid = dots < 3 ? 3 : dots;
    float half = (grid - 1) / 2.0f;
    float spacing = (size * 0.74f) / (grid - 1);
    float maxRadius = spacing * 0.6f;
    float cx = ox + FRAME_W * 0.5f;
    float cy = oy + FRAME_H * 0.5f;

    for (int iy = 0; iy < grid; iy++) {
        for (int ix = 0; ix < grid; ix++) {
            float nx = (ix - half) / half;
            float ny = (iy - half) / half;
            float d = hypotf(nx, ny);
            if (d > 1.12f) continue;   // this cutoff is what makes the outline round

            float v = clampf(intensity(d, nx, ny, t), 0.0f, 1.0f);
            float radius = maxRadius * expf(-d * d * 1.7f) * v * scale;
            if (radius < 0.5f) continue;

            Vector2 p = { cx + (ix - half) * spacing * scale,
                          cy + (iy - half) * spacing * scale };
            DrawCircleV(p, radius, color);
        }
    }
}
