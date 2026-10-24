#include "fireworks.h"
#include <math.h>
#include <string.h>

#define MAX_FIREWORKS 24
#define MAX_PARTICLES 3000
#define MAX_TRAIL     25
#define STEP          (1.0f / 60.0f)   // physics runs at a fixed 60 steps per second
#define POPULATION    1.0f             // higher means more frequent launches
#define HALF_PI_F     1.5707963f
#define TWO_PI_F      6.2831853f

typedef struct {
    float x, y, vx, vy, targetY, size;
    Color color;
    Vector2 trail[MAX_TRAIL];
    int trailCount, trailLength;
} Firework;

typedef struct {
    float x, y, vx, vy, alpha, decay, size;
    Color color;
} Particle;

static Firework fireworks[MAX_FIREWORKS];
static Particle particles[MAX_PARTICLES];
static int fireworkCount = 0, particleCount = 0;
static float launchTimer = 0, accumulator = 0, lastSt = 1.0e9f;

static Color random_color(void) {
    // hsl(h, 100%, 50%) is the same as hsv(h, 1, 1)
    return ColorFromHSV((float)GetRandomValue(0, 359), 1.0f, 1.0f);
}

static void reset(void) {
    fireworkCount = 0;
    particleCount = 0;
    launchTimer = 0;
    accumulator = 0;
}

static void spawn_firework(float x, float targetY) {
    if (fireworkCount >= MAX_FIREWORKS) return;
    Firework *f = &fireworks[fireworkCount++];
    float speed = randf(4.0f, 8.0f);
    float angle = -HALF_PI_F + randf(-0.3f, 0.3f);
    f->x = x;
    f->y = FRAME_H;
    f->vx = cosf(angle) * speed;
    f->vy = sinf(angle) * speed;
    f->targetY = targetY;
    f->size = randf(2.0f, 5.0f);
    f->color = random_color();
    f->trailCount = 0;
    f->trailLength = GetRandomValue(10, 24);
}

static void explode(const Firework *f) {
    int n = GetRandomValue(50, 149);
    for (int i = 0; i < n && particleCount < MAX_PARTICLES; i++) {
        float angle = randf(0.0f, TWO_PI_F);
        float speed = randf(2.0f, 7.0f);
        Particle *p = &particles[particleCount++];
        p->x = f->x;
        p->y = f->y;
        p->vx = cosf(angle) * speed;
        p->vy = sinf(angle) * speed;
        p->alpha = 1.0f;
        p->decay = randf(0.005f, 0.02f);
        p->size = randf(1.0f, 5.0f);
        p->color = f->color;
    }
}

static void step(void) {
    launchTimer -= STEP;
    if (launchTimer <= 0) {
        spawn_firework(randf(FRAME_W * 0.1f, FRAME_W * 0.9f),
                       randf(FRAME_H * 0.1f, FRAME_H * 0.4f));
        launchTimer = randf(0.3f, 0.8f) / POPULATION;
    }

    for (int i = fireworkCount - 1; i >= 0; i--) {
        Firework *f = &fireworks[i];
        if (f->trailCount >= f->trailLength) {
            memmove(&f->trail[0], &f->trail[1], (f->trailCount - 1) * sizeof(Vector2));
            f->trailCount--;
        }
        f->trail[f->trailCount++] = (Vector2){ f->x, f->y };

        f->x += f->vx;
        f->y += f->vy;
        f->vy += 0.02f;

        if (f->vy >= 0 || f->y <= f->targetY) {
            explode(f);
            fireworks[i] = fireworks[--fireworkCount];
        }
    }

    for (int i = particleCount - 1; i >= 0; i--) {
        Particle *p = &particles[i];
        p->vx *= 0.98f;
        p->vy *= 0.98f;
        p->vy += 0.05f;
        p->x += p->vx;
        p->y += p->vy;
        p->alpha -= p->decay;
        if (p->alpha <= 0) particles[i] = particles[--particleCount];
    }
}

// The face parameters are not used by fireworks, but main.c still eases them.
static Face fireworks_target(float t) {
    (void)t;
    return face_default();
}

static void fireworks_draw(const Face *f, float t, float st, float blink, int ox, int oy) {
    (void)f; (void)t; (void)blink;

    // The state timer restarts whenever this animation is selected
    if (st < lastSt) reset();
    lastSt = st;

    // Click inside the frame to launch a firework toward that point
    Vector2 m = GetMousePosition();
    float mx = m.x - ox, my = m.y - oy;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mx >= 0 && mx < FRAME_W && my >= 0 && my < FRAME_H)
        spawn_firework(mx, my);

    accumulator += GetFrameTime();
    if (accumulator > 0.1f) accumulator = 0.1f;
    while (accumulator >= STEP) {
        step();
        accumulator -= STEP;
    }

    // Rockets with their trails
    for (int i = 0; i < fireworkCount; i++) {
        const Firework *fw = &fireworks[i];
        Vector2 prev = { 0 };
        for (int k = 0; k < fw->trailCount; k++) {
            Vector2 p = { ox + fw->trail[k].x, oy + fw->trail[k].y };
            if (k > 0) DrawLineEx(prev, p, fw->size, fw->color);
            prev = p;
        }
        Vector2 head = { ox + fw->x, oy + fw->y };
        if (fw->trailCount > 0) DrawLineEx(prev, head, fw->size, fw->color);
        DrawCircleV(head, fw->size * 0.5f, fw->color);
    }

    // Explosion particles
    for (int i = 0; i < particleCount; i++) {
        const Particle *p = &particles[i];
        DrawCircleV((Vector2){ ox + p->x, oy + p->y }, p->size, Fade(p->color, p->alpha));
    }
}

const Animation anim_fireworks = { "Fireworks", fireworks_target, fireworks_draw };
