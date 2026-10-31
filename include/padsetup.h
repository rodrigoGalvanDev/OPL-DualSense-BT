#ifndef _PADSETUP_H_
#define _PADSETUP_H_

/*
 * "Controller Setup" (PADEMU): button remapping, PS button and touchpad behaviour.
 * Shared by the OPL GUI (EE), ee_core (which hands the blob to the IOP) and the in-game
 * pad emulator (pademu IRX). The blob is a fixed-size packed structure; the OPL config
 * stores it as a hex string (conf_opl.cfg, CONFIG_ITEM_PADEMU_SETUP).
 */

#define PADSETUP_ARG_LEN     40
#define PADSETUP_ARG_LEN_V1  28 // blobs saved before the custom lightbar colour (still accepted, the new bytes take their defaults)
#define PADSETUP_HEX_LEN (PADSETUP_ARG_LEN * 2)
#define PADSETUP_MAGIC0  'C'
#define PADSETUP_MAGIC1  'S'
#define PADSETUP_VERSION 1

// touch_mode
enum {
    PADSETUP_TOUCH_SELSTART = 0, // original behaviour: click on the left half = Select, right half = Start
    PADSETUP_TOUCH_BUTTON = 1,   // click = touch_click button, no touch position handling
    PADSETUP_TOUCH_RSTICK_DRAG = 2,
    PADSETUP_TOUCH_LSTICK_DRAG = 3,
    PADSETUP_TOUCH_RSTICK_MOUSE = 4,
    PADSETUP_TOUCH_LSTICK_MOUSE = 5,
    PADSETUP_TOUCH_MODE_COUNT = 6,
};

// ps_hold (long press of the PS button, emulates the OPL in-game reset combos)
enum {
    PADSETUP_PSHOLD_NONE = 0,
    PADSETUP_PSHOLD_EXIT = 1,       // L1+L2+R1+R2+Start+Select: back to the OPL menu
    PADSETUP_PSHOLD_POWEROFF = 2,   // L1+L2+R1+R2+L3+R3: power off
    PADSETUP_PSHOLD_SCREENSHOT = 3, // L1+L2+R1+R2+Up: in-game screenshot (GSM/IGS)
    PADSETUP_PSHOLD_PADOFF = 4,     // disconnect the pad, which switches it off (default; PS reconnects it)
    PADSETUP_PSHOLD_COUNT = 5,
};

// Button targets: 0 = no change/none, 1..16 = DS2 button bit number + 1 (select, l3, r3, start, up, right, down,
// left, l2, r2, l1, r1, triangle, circle, cross, square). For remap[] a value of 0xFF disables the button.
#define PADSETUP_TARGET_DISABLED 0xFF

// Lightbar colour of each of the 4 slots: index into this palette, 0 = default pattern of the slot.
// Stored in led[2] (slot 0 = low nibble of led[0], ... slot 3 = high nibble of led[1]); it travels to the in-game pad emulator with the rest of the blob.
#define PADSETUP_LED_SLOTS 4
#define PADSETUP_LED_COUNT 11 // Default, Red, Green, Blue, Yellow, Cyan, Magenta, White, Orange, Off, Custom
#define PADSETUP_LED_CUSTOM 10 // colour picked in Controller Setup for that slot (led_custom[slot * 3] of the blob)

static inline const unsigned char *padsetup_led_rgb(int idx)
{
    static const unsigned char rgb[PADSETUP_LED_COUNT][3] = {
        {0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 200, 0}, {0, 255, 255}, {255, 0, 255}, {255, 255, 255}, {255, 90, 0}, {0, 0, 0}, {0, 0, 0}};

    return rgb[(idx > 0 && idx < PADSETUP_LED_COUNT) ? idx : 0];
}

typedef struct
{
    unsigned char magic[2];
    unsigned char version;
    unsigned char touch_mode;
    unsigned char touch_sens;    // 1..8
    unsigned char touch_click;   // button pressed by the touchpad click (modes other than SELSTART)
    unsigned char ps_tap;        // button pulsed by a short press of the PS button
    unsigned char ps_hold;       // PADSETUP_PSHOLD_*
    unsigned char ps_hold_time;  // 0 = 1s, 1 = 2s, 2 = 3s
    unsigned char reserved0;
    unsigned char remap[16];     // per DS2 button: 0 = unchanged, 0xFF = disabled, else target + 1
    unsigned char led[2];        // lightbar colour per slot (DS4/DualSense), 4 bits each, see padsetup_get_led()
    unsigned char led_custom[12]; // R, G, B of each slot, used when the slot (or its pad) is set to "Custom"
} __attribute__((packed)) padsetup_t;

_Static_assert(sizeof(padsetup_t) == PADSETUP_ARG_LEN, "padsetup_t size");

// RGB of a palette entry of this setup (the Custom entry reads the colour picked by the user)
static inline void padsetup_led_rgb_of(const padsetup_t *s, int slot, int idx, unsigned char *rgb)
{
    const unsigned char *c = (idx == PADSETUP_LED_CUSTOM && slot >= 0 && slot < PADSETUP_LED_SLOTS) ? &s->led_custom[slot * 3] : padsetup_led_rgb(idx);

    rgb[0] = c[0];
    rgb[1] = c[1];
    rgb[2] = c[2];
}

/*
 * Parameter table: every single-value setting of the blob is described here once (where it lives, its valid range and
 * its default). The OPL menu (src/guigame.c), the config load/save and the in-game emulator (bt_pademu) all go through
 * it, so adding a setting or redesigning the menu does not mean touching three places:
 *   1. add the field to padsetup_t (use a reserved byte, the blob size and PADSETUP_VERSION stay the same),
 *   2. add an enum value below and one row in padsetup_params[],
 *   3. add a row in the GUI table (gSetupUi[] in src/guigame.c) and the dialog item (src/dialogs.c).
 * Button remapping (16 bytes) is not a single value and has its own accessors (padsetup_get_remap / set_remap).
 */
enum {
    PADSETUP_P_TOUCH_MODE = 0,
    PADSETUP_P_TOUCH_SENS,
    PADSETUP_P_TOUCH_CLICK,
    PADSETUP_P_PS_TAP,
    PADSETUP_P_PS_HOLD,
    PADSETUP_P_PS_TIME,
    PADSETUP_P_LED0, // lightbar colour of slots 1 to 4 (consecutive: PADSETUP_P_LED0 + slot)
    PADSETUP_P_LED1,
    PADSETUP_P_LED2,
    PADSETUP_P_LED3,
    PADSETUP_P_COUNT,
};

typedef struct
{
    unsigned char off;   // byte offset inside padsetup_t
    unsigned char shift; // bit position of the value inside that byte
    unsigned char mask;  // value mask (after the shift)
    unsigned char min;
    unsigned char max;
    unsigned char def;
} padsetup_param_t;

#define PADSETUP_PARAM_ROW(field, shift, mask, min, max, def) {(unsigned char)__builtin_offsetof(padsetup_t, field), shift, mask, min, max, def}

static const padsetup_param_t padsetup_params[PADSETUP_P_COUNT] = {
    /* TOUCH_MODE  */ PADSETUP_PARAM_ROW(touch_mode, 0, 0xFF, 0, PADSETUP_TOUCH_MODE_COUNT - 1, PADSETUP_TOUCH_SELSTART),
    /* TOUCH_SENS  */ PADSETUP_PARAM_ROW(touch_sens, 0, 0xFF, 1, 8, 4),
    /* TOUCH_CLICK */ PADSETUP_PARAM_ROW(touch_click, 0, 0xFF, 0, 16, 0),
    /* PS_TAP      */ PADSETUP_PARAM_ROW(ps_tap, 0, 0xFF, 0, 16, 0),
    /* PS_HOLD     */ PADSETUP_PARAM_ROW(ps_hold, 0, 0xFF, 0, PADSETUP_PSHOLD_COUNT - 1, PADSETUP_PSHOLD_PADOFF), // like a PS4/PS5: hold PS to switch the pad off
    /* PS_TIME     */ PADSETUP_PARAM_ROW(ps_hold_time, 0, 0xFF, 0, 2, 1),                                       // 2 seconds
    /* LED0        */ PADSETUP_PARAM_ROW(led[0], 0, 0x0F, 0, PADSETUP_LED_COUNT - 1, 0),
    /* LED1        */ PADSETUP_PARAM_ROW(led[0], 4, 0x0F, 0, PADSETUP_LED_COUNT - 1, 0),
    /* LED2        */ PADSETUP_PARAM_ROW(led[1], 0, 0x0F, 0, PADSETUP_LED_COUNT - 1, 0),
    /* LED3        */ PADSETUP_PARAM_ROW(led[1], 4, 0x0F, 0, PADSETUP_LED_COUNT - 1, 0),
};

// Raw value of a parameter (not range checked). Out-of-range parameter numbers read as 0.
static inline int padsetup_get_raw(const padsetup_t *s, int p)
{
    const padsetup_param_t *d;

    if (p < 0 || p >= PADSETUP_P_COUNT)
        return 0;
    d = &padsetup_params[p];
    return (((const unsigned char *)s)[d->off] >> d->shift) & d->mask;
}

// Current value; a stored value outside the valid range reads as the default.
static inline int padsetup_get(const padsetup_t *s, int p)
{
    int v;

    if (p < 0 || p >= PADSETUP_P_COUNT)
        return 0;
    v = padsetup_get_raw(s, p);
    return (v < padsetup_params[p].min || v > padsetup_params[p].max) ? padsetup_params[p].def : v;
}

// Sets a parameter. Returns 1 when stored, 0 (blob unchanged) when the value is outside the valid range.
static inline int padsetup_set(padsetup_t *s, int p, int v)
{
    const padsetup_param_t *d;
    unsigned char *b;

    if (p < 0 || p >= PADSETUP_P_COUNT)
        return 0;
    d = &padsetup_params[p];
    if (v < d->min || v > d->max)
        return 0;
    b = &((unsigned char *)s)[d->off];
    *b = (unsigned char)((*b & ~(d->mask << d->shift)) | (v << d->shift));
    return 1;
}

// Puts every stored value that is out of range back to its default (blob from an older/newer version or corrupted).
// One pass over the table, written to stay small: this runs inside bt_pademu, where every byte is IOP RAM.
static inline void padsetup_sanitize(padsetup_t *s)
{
    const padsetup_param_t *d;

    for (d = padsetup_params; d != padsetup_params + PADSETUP_P_COUNT; d++) {
        unsigned char *b = &((unsigned char *)s)[d->off];
        int v = (*b >> d->shift) & d->mask;

        if (v < d->min || v > d->max)
            *b = (unsigned char)((*b & ~(d->mask << d->shift)) | (d->def << d->shift));
    }
}

// Lightbar colour of a slot, 0 = default pattern of the slot (kept for the bt_pademu / menu code)
static inline int padsetup_get_led(const padsetup_t *s, int slot)
{
    int v;

    if (slot < 0 || slot >= PADSETUP_LED_SLOTS)
        return 0;
    v = (s->led[slot >> 1] >> ((slot & 1) * 4)) & 0x0F;
    return v < PADSETUP_LED_COUNT ? v : 0; // anything unknown = default
}

static inline void padsetup_set_led(padsetup_t *s, int slot, int idx)
{
    int sh = (slot & 1) * 4;

    if (slot < 0 || slot >= PADSETUP_LED_SLOTS)
        return;
    if (idx < 0 || idx >= PADSETUP_LED_COUNT)
        idx = 0;
    s->led[slot >> 1] = (unsigned char)((s->led[slot >> 1] & ~(0x0F << sh)) | (idx << sh));
}

/*
 * Button remapping. Stored value per DS2 button: 0 = unchanged, 0xFF = disabled, 1..16 = target button + 1.
 * UI index (what a dropdown holds): 0 = Unchanged, 1 = Disabled, 2..17 = target button (Select..Square).
 */
static inline int padsetup_get_remap(const padsetup_t *s, int btn)
{
    unsigned char v;

    if (btn < 0 || btn >= 16)
        return 0;
    v = s->remap[btn];
    if (v == 0)
        return 0;
    if (v == PADSETUP_TARGET_DISABLED)
        return 1;
    return (v >= 1 && v <= 16) ? v + 1 : 0;
}

static inline void padsetup_set_remap(padsetup_t *s, int btn, int idx)
{
    if (btn < 0 || btn >= 16)
        return;
    s->remap[btn] = idx <= 0 ? 0 : (idx == 1 ? PADSETUP_TARGET_DISABLED : (unsigned char)(idx - 1));
}

static inline void padsetup_default(padsetup_t *s)
{
    unsigned char *p = (unsigned char *)s;
    int i;

    for (i = 0; i < PADSETUP_ARG_LEN; i++)
        p[i] = 0;
    s->magic[0] = PADSETUP_MAGIC0;
    s->magic[1] = PADSETUP_MAGIC1;
    s->version = PADSETUP_VERSION;
    for (i = 0; i < PADSETUP_LED_SLOTS; i++) { // default custom colour of every slot: violet
        s->led_custom[i * 3] = 128;
        s->led_custom[i * 3 + 1] = 0;
        s->led_custom[i * 3 + 2] = 255;
    }
    for (i = 0; i < PADSETUP_P_COUNT; i++)
        p[padsetup_params[i].off] |= (unsigned char)(padsetup_params[i].def << padsetup_params[i].shift);
}

static inline int padsetup_valid(const padsetup_t *s)
{
    return s->magic[0] == PADSETUP_MAGIC0 && s->magic[1] == PADSETUP_MAGIC1 && s->version == PADSETUP_VERSION;
}

static inline int padsetup_is_default(const padsetup_t *s)
{
    padsetup_t d;
    const unsigned char *a = (const unsigned char *)s, *b = (const unsigned char *)&d;
    int i;

    padsetup_default(&d);
    for (i = 0; i < PADSETUP_ARG_LEN; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static inline void padsetup_to_hex(const padsetup_t *s, char *out)
{
    static const char hex[] = "0123456789ABCDEF";
    const unsigned char *p = (const unsigned char *)s;
    int i;

    for (i = 0; i < PADSETUP_ARG_LEN; i++) {
        out[2 * i] = hex[p[i] >> 4];
        out[2 * i + 1] = hex[p[i] & 15];
    }
    out[PADSETUP_HEX_LEN] = '\0';
}

// Returns 1 and fills *s when str holds a valid blob; otherwise *s is the default setup and 0 is returned.
static inline int padsetup_from_hex(const char *str, padsetup_t *s)
{
    unsigned char *p = (unsigned char *)s;
    int i, hi, lo;

    padsetup_default(s);
    for (i = 0; i < PADSETUP_ARG_LEN; i++) {
        char a = str[2 * i], b;
        if (a == '\0') {
            if (i == PADSETUP_ARG_LEN_V1)
                break; // blob from before the custom colour: the rest keeps its defaults
            goto bad;
        }
        b = str[2 * i + 1];
        hi = (a >= '0' && a <= '9') ? a - '0' : (a >= 'A' && a <= 'F') ? a - 'A' + 10 : (a >= 'a' && a <= 'f') ? a - 'a' + 10 : -1;
        lo = (b >= '0' && b <= '9') ? b - '0' : (b >= 'A' && b <= 'F') ? b - 'A' + 10 : (b >= 'a' && b <= 'f') ? b - 'a' + 10 : -1;
        if (hi < 0 || lo < 0)
            goto bad;
        p[i] = (unsigned char)((hi << 4) | lo);
    }
    if (!padsetup_valid(s))
        goto bad;
    padsetup_sanitize(s);
    return 1;
bad:
    padsetup_default(s);
    return 0;
}

#endif
