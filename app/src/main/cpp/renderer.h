// renderer.h
// Minimal software rasterizer: fills rectangles and draws bitmap-font text
// directly into an ANativeWindow's pixel buffer. No Skia/Canvas/Java drawing
// APIs are used - this is why an ANativeWindow (native surface) is required
// instead of a normal Android View.
#ifndef QRKB_RENDERER_H
#define QRKB_RENDERER_H

#include <android/native_window.h>
#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

// Draws the full keyboard (background + every key + label) into `window`.
// `pressed_idx` (or -1) is drawn with a highlighted fill for touch feedback.
void render_keyboard(ANativeWindow *window, const kb_state_t *kb, int pressed_idx);

#ifdef __cplusplus
}
#endif

#endif // QRKB_RENDERER_H
