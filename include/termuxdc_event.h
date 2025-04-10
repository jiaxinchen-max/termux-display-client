#ifndef INPUT_EVENT_H
#define INPUT_EVENT_H
#include <stdint.h>
#ifndef EVENT_TYPE_ENUM
#define EVENT_TYPE_ENUM
typedef enum {
    NONE,
    EVENT_SCREEN_SIZE,
    EVENT_TOUCH,
    EVENT_MOUSE,
    EVENT_KEY,
    EVENT_STYLUS,
    EVENT_STYLUS_ENABLE,
    EVENT_UNICODE,
    EVENT_CLIPBOARD_ENABLE,
    EVENT_CLIPBOARD_ANNOUNCE,
    EVENT_CLIPBOARD_REQUEST,
    EVENT_CLIPBOARD_SEND,
    EVENT_CLIENT_EXIT,
    EVENT_FRAME_COMPLETE,
    EVENT_TOUCH_DOWN,
    EVENT_TOUCH_UP,
    EVENT_TOUCH_MOVE,
    EVENT_TOUCH_POINTER_UP,
    EVENT_DRAW_FRAME,
} event_type;
#endif
#ifndef TERMUX_EVENT
#define TERMUX_EVENT
typedef struct {
    uint8_t num_pointers;
    uint8_t t;
    uint16_t type, id, x, y;
} touch_event;
typedef union {
    uint8_t type;
    struct {
        uint8_t t;
        uint16_t width, height, framerate;
    } screenSize;
    touch_event touch;
    touch_event touch_events[4];
    struct {
        uint8_t t;
        float x, y;
        uint8_t detail, down, relative;
    } mouse;
    struct {
        uint8_t t;
        uint16_t key;
        uint8_t state;
        uint8_t mod;
    } key;
    struct {
        uint8_t t;
        float x, y;
        uint16_t pressure;
        int8_t tilt_x, tilt_y;
        int16_t orientation;
        uint8_t buttons, eraser, mouse;
    } stylus;
    struct {
        uint8_t t, enable;
    } stylusEnable;
    struct {
        uint8_t t;
        uint32_t code;
    } unicode;
    struct {
        uint8_t t;
        uint8_t enable;
    } clipboardEnable;
    struct {
        uint8_t t;
        uint32_t count;
    } clipboardSend;
    struct {
        uint64_t timestamp;
    } frame;
} termuxdc_event;
#endif
typedef enum {
    /// No modifier pressed.
    TDC_MOD_NONE = 0,
    /// Left shift pressed.
    TDC_MOD_LSHIFT = 1,
    /// Right shift pressed.
    TDC_MOD_RSHIFT = 2,
    /// Left ctrl pressed.
    TDC_MOD_LCTRL = 4,
    /// Right ctrl pressed.
    TDC_MOD_RCTRL = 8,
    /// Alt pressed.
    TDC_MOD_ALT = 16,
    /// Fn pressed.
    TDC_MOD_FN = 32,
    /// Caps lock pressed.
    TDC_MOD_CAPS_LOCK = 64,
    /// Alt gr pressed.
    TDC_MOD_ALT_GR = 128,
    /// Num lock pressed.
    TDC_MOD_NUM_LOCK = 256,

    /// Maximum modifier value, for future binary compatibility.
    TDC_MOD_MAX = 1 << 31,
} termuxdc_key_modifier;
#endif
