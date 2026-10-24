#include "oled.h"
#include "common/common.h"

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define PANEL_W        128
#define PANEL_H        64
#define FRAME_BYTES   (PANEL_W * PANEL_H / 8)
#define RETRY_SECONDS 3.0     // wait between connection attempts
#define HELLO_SECONDS 4.0     // the UNO reboots when the port opens, give it time
#define GAIN          1.5f    // thin lines get dim after shrinking, boost them
#define BLACK_LEVEL         24      // the dark background must not turn into dither dots

// Bytes from the board: 'H' hello (booted), 'R' ready for a frame, 'B' button pressed.
static int    fd = -1;
static bool   linked, ready, sentAny;
static double openedAt, lastTry = -RETRY_SECONDS;
static unsigned char pending[FRAME_BYTES];
static int    pendingOff = FRAME_BYTES;

static const unsigned char BAYER[4][4] = {
    {  0,  8,  2, 10 }, { 12,  4, 14,  6 }, {  3, 11,  1,  9 }, { 15,  7, 13,  5 }
};

static void drop(void) {
    if (fd >= 0) close(fd);
    if (linked) { printf("OLED disconnected\n"); fflush(stdout); }
    fd = -1;
    linked = ready = sentAny = false;
    pendingOff = FRAME_BYTES;
}

static int open_port(const char *path) {
    int f = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (f < 0) return -1;
    struct termios tio;
    if (tcgetattr(f, &tio) != 0) { close(f); return -1; }
    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;
    cfsetspeed(&tio, B1000000);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(f, TCSANOW, &tio) != 0) { close(f); return -1; }
    tcflush(f, TCIOFLUSH);
    return f;
}

static void try_open(double now) {
    const char *forced = getenv("OLED_PORT");
    if (forced) { fd = open_port(forced); }
    else {
        glob_t g;
        if (glob("/dev/ttyACM*", 0, NULL, &g) != 0) g.gl_pathc = 0;
        if (g.gl_pathc == 0) {
            globfree(&g);
            if (glob("/dev/ttyUSB*", 0, NULL, &g) != 0) g.gl_pathc = 0;
        }
        if (g.gl_pathc > 0) fd = open_port(g.gl_pathv[0]);
        globfree(&g);
    }
    openedAt = now;
}

int oled_poll(void) {
    double now = GetTime();
    if (fd < 0) {
        if (now - lastTry >= RETRY_SECONDS) { lastTry = now; try_open(now); }
        return 0;
    }

    if (pendingOff < FRAME_BYTES) {
        ssize_t n = write(fd, pending + pendingOff, FRAME_BYTES - pendingOff);
        if (n > 0) pendingOff += (int)n;
        else if (n < 0 && errno != EAGAIN) { drop(); return 0; }
    }

    int presses = 0;
    unsigned char buf[64];
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            if (buf[i] == 'B') presses++;
            else if (buf[i] == 'R' || (buf[i] == 'H' && !sentAny)) {
                if (!linked) { linked = true; printf("OLED connected\n"); fflush(stdout); }
                ready = true;
            }
        }
    }
    if (n < 0 && errno != EAGAIN) { drop(); return presses; }

    if (!linked && now - openedAt > HELLO_SECONDS) drop();   // not our board
    return presses;
}

bool oled_ready(void) {
    return fd >= 0 && ready && pendingOff >= FRAME_BYTES;
}

void oled_send(const Image *img) {
    if (img->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) return;
    const unsigned char *px = img->data;

    // Fit the 4:3 frame into the 2:1 panel by height, centered. Source pixels per panel pixel:
    float s = (float)img->height / PANEL_H;
    int x0 = (int)((PANEL_W - img->width / s) * 0.5f);
    int x1 = PANEL_W - x0;

    memset(pending, 0, sizeof pending);
    for (int dy = 0; dy < PANEL_H; dy++) {
        // Render textures come back upside down
        int sy0 = img->height - (int)((dy + 1) * s), sy1 = img->height - (int)(dy * s);
        for (int dx = x0; dx < x1; dx++) {
            int sx0 = (int)((dx - x0) * s), sx1 = (int)((dx - x0 + 1) * s);
            if (sx1 > img->width) sx1 = img->width;
            long sum = 0; int cnt = 0;
            for (int y = sy0; y < sy1; y++)
                for (int x = sx0; x < sx1; x++) {
                    const unsigned char *p = px + ((size_t)y * img->width + x) * 4;
                    int m = p[0] > p[1] ? p[0] : p[1];
                    sum += m > p[2] ? m : p[2];
                    cnt++;
                }
            float v = cnt ? clampf((sum / (float)cnt - BLACK_LEVEL) / 255.0f * GAIN, 0, 1) : 0;
            if (v * 16 > BAYER[dy & 3][dx & 3] + 0.5f)
                pending[dx + (dy / 8) * PANEL_W] |= 1 << (dy & 7);
        }
    }
    pendingOff = 0;
    sentAny = true;
    ready = false;
}

void oled_close(void) { drop(); }
