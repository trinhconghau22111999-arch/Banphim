// keyboard.c
#include "keyboard.h"
#include <string.h>

// --- Layout tables -----------------------------------------------------
// Each row is a list of {lower_label, upper_label, action, weight}.
// weight = relative width (1.0 = one normal key). Rows are laid out to
// fill the full keyboard width; row height is fixed (KB_ROWS rows fit in
// the given surface height).

typedef struct {
    const char *lo;
    const char *up;
    kb_action_t action;
    float weight;
} kb_template_key_t;

#define ROW(...) { __VA_ARGS__, {NULL,NULL,KA_NONE,0} }

static const kb_template_key_t ROW_LETTERS_0[] =
    ROW({"q","Q",KA_CHAR,1},{"w","W",KA_CHAR,1},{"e","E",KA_CHAR,1},
        {"r","R",KA_CHAR,1},{"t","T",KA_CHAR,1},{"y","Y",KA_CHAR,1},
        {"u","U",KA_CHAR,1},{"i","I",KA_CHAR,1},{"o","O",KA_CHAR,1},
        {"p","P",KA_CHAR,1});

static const kb_template_key_t ROW_LETTERS_1[] =
    ROW({"a","A",KA_CHAR,1},{"s","S",KA_CHAR,1},{"d","D",KA_CHAR,1},
        {"f","F",KA_CHAR,1},{"g","G",KA_CHAR,1},{"h","H",KA_CHAR,1},
        {"j","J",KA_CHAR,1},{"k","K",KA_CHAR,1},{"l","L",KA_CHAR,1});

static const kb_template_key_t ROW_LETTERS_2[] =
    ROW({"^","^",KA_SHIFT,1.5f},{"z","Z",KA_CHAR,1},{"x","X",KA_CHAR,1},
        {"c","C",KA_CHAR,1},{"v","V",KA_CHAR,1},{"b","B",KA_CHAR,1},
        {"n","N",KA_CHAR,1},{"m","M",KA_CHAR,1},{"<-","<-",KA_BACKSPACE,1.5f});

static const kb_template_key_t ROW_BOTTOM[] =
    ROW({"123","123",KA_MODE_SYMBOLS,1.3f},{",",",",KA_CHAR,1},
        {"QR","QR",KA_QR_SCAN,1.3f},{" "," ",KA_SPACE,3.4f},
        {".",".",KA_CHAR,1},{"OK","OK",KA_ENTER,1.5f});

static const kb_template_key_t ROW_SYMBOLS_0[] =
    ROW({"1","1",KA_CHAR,1},{"2","2",KA_CHAR,1},{"3","3",KA_CHAR,1},
        {"4","4",KA_CHAR,1},{"5","5",KA_CHAR,1},{"6","6",KA_CHAR,1},
        {"7","7",KA_CHAR,1},{"8","8",KA_CHAR,1},{"9","9",KA_CHAR,1},
        {"0","0",KA_CHAR,1});

static const kb_template_key_t ROW_SYMBOLS_1[] =
    ROW({"@","@",KA_CHAR,1},{"#","#",KA_CHAR,1},{"$","$",KA_CHAR,1},
        {"_","_",KA_CHAR,1},{"&","&",KA_CHAR,1},{"-","-",KA_CHAR,1},
        {"+","+",KA_CHAR,1},{"(","(",KA_CHAR,1},{")",")",KA_CHAR,1});

static const kb_template_key_t ROW_SYMBOLS_2[] =
    ROW({"*","*",KA_CHAR,1.5f},{"\"","\"",KA_CHAR,1},{"'","'",KA_CHAR,1},
        {":",":",KA_CHAR,1},{";",";",KA_CHAR,1},{"!","!",KA_CHAR,1},
        {"?","?",KA_CHAR,1},{"<-","<-",KA_BACKSPACE,1.5f});

static const kb_template_key_t ROW_BOTTOM_SYM[] =
    ROW({"ABC","ABC",KA_MODE_LETTERS,1.3f},{",",",",KA_CHAR,1},
        {"QR","QR",KA_QR_SCAN,1.3f},{" "," ",KA_SPACE,3.4f},
        {".",".",KA_CHAR,1},{"OK","OK",KA_ENTER,1.5f});

typedef struct {
    const kb_template_key_t *rows[4];
} kb_page_t;

static const kb_page_t PAGE_LETTERS = { { ROW_LETTERS_0, ROW_LETTERS_1, ROW_LETTERS_2, ROW_BOTTOM } };
static const kb_page_t PAGE_SYMBOLS = { { ROW_SYMBOLS_0, ROW_SYMBOLS_1, ROW_SYMBOLS_2, ROW_BOTTOM_SYM } };

// --- API -----------------------------------------------------------------

void kb_init(kb_state_t *kb) {
    memset(kb, 0, sizeof(*kb));
    kb->shift_on = false;
    kb->symbols_mode = false;
    kb->width = 0;
    kb->height = 0;
}

void kb_layout(kb_state_t *kb, float width, float height) {
    kb->width = width;
    kb->height = height;

    const kb_page_t *page = kb->symbols_mode ? &PAGE_SYMBOLS : &PAGE_LETTERS;
    const int row_count = 4;
    const float row_h = height / row_count;

    int n = 0;
    for (int r = 0; r < row_count; r++) {
        const kb_template_key_t *row = page->rows[r];
        // total weight
        float total_w = 0;
        int count = 0;
        for (const kb_template_key_t *k = row; k->action != KA_NONE || k->lo != NULL; k++) {
            if (k->lo == NULL) break;
            total_w += k->weight;
            count++;
        }
        float unit = width / (total_w > 0 ? total_w : 1);
        float cx = 0;
        for (int i = 0; i < count; i++) {
            const kb_template_key_t *k = &row[i];
            if (n >= KB_MAX_KEYS) break;
            kb_key_t *dst = &kb->keys[n];
            strncpy(dst->label, k->lo, KB_LABEL_LEN - 1);
            dst->label[KB_LABEL_LEN - 1] = 0;
            strncpy(dst->label_shift, k->up, KB_LABEL_LEN - 1);
            dst->label_shift[KB_LABEL_LEN - 1] = 0;
            dst->action = k->action;
            dst->x = cx;
            dst->y = r * row_h;
            dst->w = unit * k->weight;
            dst->h = row_h;
            cx += dst->w;
            n++;
        }
    }
    kb->key_count = n;
}

int kb_hit_test(const kb_state_t *kb, float x, float y) {
    for (int i = 0; i < kb->key_count; i++) {
        const kb_key_t *k = &kb->keys[i];
        if (x >= k->x && x < k->x + k->w && y >= k->y && y < k->y + k->h) {
            return i;
        }
    }
    return -1;
}

const kb_key_t *kb_tap(kb_state_t *kb, int idx) {
    if (idx < 0 || idx >= kb->key_count) return NULL;
    // Copy out before any potential re-layout invalidates the array.
    static kb_key_t tapped;
    tapped = kb->keys[idx];
    bool shift_was_on = kb->shift_on;

    switch (tapped.action) {
        case KA_SHIFT:
            kb->shift_on = !kb->shift_on;
            kb_layout(kb, kb->width, kb->height);
            break;
        case KA_MODE_SYMBOLS:
            kb->symbols_mode = true;
            kb_layout(kb, kb->width, kb->height);
            break;
        case KA_MODE_LETTERS:
            kb->symbols_mode = false;
            kb_layout(kb, kb->width, kb->height);
            break;
        case KA_CHAR:
            // Resolve the actual character to commit NOW, using the shift
            // state at the moment of the tap, and bake it into `label` so
            // the JNI bridge can just read key->label uniformly.
            if (shift_was_on) {
                strncpy(tapped.label, tapped.label_shift, KB_LABEL_LEN - 1);
                tapped.label[KB_LABEL_LEN - 1] = 0;
                // one-shot shift, like a normal mobile keyboard
                kb->shift_on = false;
                kb_layout(kb, kb->width, kb->height);
            }
            break;
        default:
            break;
    }
    return &tapped;
}
