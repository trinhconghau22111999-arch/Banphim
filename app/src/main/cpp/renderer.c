// renderer.c
#include "renderer.h"
#include "font5x7.h"
#include <stdint.h>
#include <string.h>

typedef struct { uint8_t r, g, b, a; } px_t;

static inline void put_px(uint32_t *row, int x, px_t c) {
    row[x] = (c.a << 24) | (c.b << 16) | (c.g << 8) | c.r; // RGBA_8888 little-endian layout
}

static void fill_rect(ANativeWindow_Buffer *buf, int x0, int y0, int x1, int y1, px_t c) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > buf->width) x1 = buf->width;
    if (y1 > buf->height) y1 = buf->height;
    uint32_t *pixels = (uint32_t *)buf->bits;
    for (int y = y0; y < y1; y++) {
        uint32_t *row = pixels + y * buf->stride;
        for (int x = x0; x < x1; x++) put_px(row, x, c);
    }
}

static void stroke_rect(ANativeWindow_Buffer *buf, int x0, int y0, int x1, int y1, px_t c, int t) {
    fill_rect(buf, x0, y0, x1, y0 + t, c);
    fill_rect(buf, x0, y1 - t, x1, y1, c);
    fill_rect(buf, x0, y0, x0 + t, y1, c);
    fill_rect(buf, x1 - t, y0, x1, y1, c);
}

// Draws one glyph, scaled, top-left at (x,y). Uses the classic Adafruit
// 5x7 column-major bitmap font (5 bytes/char, bit0=top .. bit6=bottom).
static void draw_char(ANativeWindow_Buffer *buf, int x, int y, char ch, int scale, px_t c) {
    unsigned char uc = (unsigned char)ch;
    if (uc < 0x20 || uc > 0x7E) return;
    // font5x7.h is the full 256-glyph Adafruit table (starts at code 0x00),
    // so index by the raw char code -- do NOT subtract 0x20.
    const uint8_t *glyph = &font[uc * 5];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                fill_rect(buf, x + col * scale, y + row * scale,
                          x + col * scale + scale, y + row * scale + scale, c);
            }
        }
    }
}

static int text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    return n > 0 ? n * 6 * scale - scale : 0; // 5 px glyph + 1 px gap, minus trailing gap
}

static void draw_text_centered(ANativeWindow_Buffer *buf, float cx, float cy,
                                const char *s, int scale, px_t c) {
    int w = text_width(s, scale);
    int h = 7 * scale;
    int x = (int)(cx - w / 2.0f);
    int y = (int)(cy - h / 2.0f);
    for (const char *p = s; *p; p++) {
        draw_char(buf, x, y, *p, scale, c);
        x += 6 * scale;
    }
}

void render_keyboard(ANativeWindow *window, const kb_state_t *kb, int pressed_idx) {
    if (!window) return;
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(window, &buf, NULL) != 0) return;

    const px_t bg       = {30, 33, 39, 255};
    const px_t key_bg   = {56, 60, 68, 255};
    const px_t key_down = {70, 130, 200, 255};
    const px_t key_qr   = {46, 125, 90, 255};
    const px_t border   = {20, 22, 26, 255};
    const px_t text_c   = {235, 235, 235, 255};

    fill_rect(&buf, 0, 0, buf.width, buf.height, bg);

    for (int i = 0; i < kb->key_count; i++) {
        const kb_key_t *k = &kb->keys[i];
        int x0 = (int)k->x + 2, y0 = (int)k->y + 2;
        int x1 = (int)(k->x + k->w) - 2, y1 = (int)(k->y + k->h) - 2;

        px_t fill = (i == pressed_idx) ? key_down
                    : (k->action == KA_QR_SCAN) ? key_qr
                    : key_bg;
        fill_rect(&buf, x0, y0, x1, y1, fill);
        stroke_rect(&buf, x0, y0, x1, y1, border, 1);

        const char *label = kb->shift_on ? k->label_shift : k->label;
        int scale = (k->w < k->h) ? 2 : 3; // keep glyphs from overflowing narrow keys
        if (scale > 3) scale = 3;
        draw_text_centered(&buf, k->x + k->w / 2.0f, k->y + k->h / 2.0f, label, scale, text_c);
    }

    ANativeWindow_unlockAndPost(window);
}
