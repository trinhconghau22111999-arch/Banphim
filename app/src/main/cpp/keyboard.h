// keyboard.h
// Pure C keyboard layout + state machine.
// This replaces 100% of the "draw a QWERTY keyboard with Shift / 123-ABC /
// space / enter / backspace / QR button" logic that would otherwise live in
// a Kotlin InputMethodService. No Android types appear in this file.
#ifndef QRKB_KEYBOARD_H
#define QRKB_KEYBOARD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KB_MAX_KEYS 64
#define KB_LABEL_LEN 8

typedef enum {
    KA_NONE = 0,
    KA_CHAR,        // commit kb_key.label (respecting shift) as text
    KA_SHIFT,       // toggle shift state
    KA_MODE_SYMBOLS,// switch to symbols/number page
    KA_MODE_LETTERS,// switch back to letters page
    KA_SPACE,
    KA_ENTER,
    KA_BACKSPACE,
    KA_QR_SCAN      // launch the QR scanner
} kb_action_t;

typedef struct {
    char label[KB_LABEL_LEN];   // what to draw / what to commit for KA_CHAR
    char label_shift[KB_LABEL_LEN]; // label when shift is on (upper-case form)
    kb_action_t action;
    float x, y, w, h;           // pixel rect, computed by kb_layout()
} kb_key_t;

typedef struct {
    bool shift_on;
    bool symbols_mode;
    int  key_count;
    kb_key_t keys[KB_MAX_KEYS];
    float width, height;        // last known surface size
} kb_state_t;

// Resets to the default (letters, no shift) layout.
void kb_init(kb_state_t *kb);

// Recomputes key pixel rects for the given surface size. Call whenever the
// surface size changes (rotation, IME shown, etc).
void kb_layout(kb_state_t *kb, float width, float height);

// Returns the index of the key under (x,y), or -1 if none.
int kb_hit_test(const kb_state_t *kb, float x, float y);

// Applies the tap on key index `idx`. Updates internal state (shift / mode)
// and returns the resulting key by pointer so the caller (JNI bridge) can
// react (commit text, delete, enter, start QR scan...). Returns NULL if idx
// is invalid. NOTE: this may re-layout the keyboard (mode switch), so the
// caller must not hold onto stale key pointers/indices afterward.
const kb_key_t *kb_tap(kb_state_t *kb, int idx);

#ifdef __cplusplus
}
#endif

#endif // QRKB_KEYBOARD_H
