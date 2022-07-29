#ifndef CH559_H_
#define CH559_H_

#include <stdio.h>
#include <machine.h>
#include <basicio.h>
#include <printf.h>
#include <string.h>

#define KEY_CAPSLOCK 0x39

#define DIR_DEAD        8
#define DIR_UP          0
#define DIR_UP_RIGHT    1
#define DIR_RIGHT       2
#define DIR_DOWN_RIGHT  3
#define DIR_DOWN        4
#define DIR_DOWN_LEFT   5
#define DIR_LEFT        6
#define DIR_UP_LEFT     7

#define USB_MSG_CONNECT     0x01
#define USB_MSG_DISCONNECT  0x02
#define USB_MSG_ERROR       0x03
#define USB_MSG_REPORT      0x04
#define USB_MSG_DEV_DESC    0x05
#define USB_MSG_DEV_INFO    0x06
#define USB_MSG_DESCRIPTOR  0x07
#define USB_MSG_STARTUP     0x08

#define TYPE_UNKNOWN	    0x00
#define TYPE_MOUSE		    0x02
#define TYPE_JOYSTICK	    0x04
#define TYPE_GAMEPAD	    0x05
#define TYPE_KEYBOARD	    0x06

#define MAX_DEVICES         8

typedef enum {
    STATE_DISCARD,
    STATE_AWAIT_SIG_1,
    STATE_AWAIT_LEN_HI,
    STATE_AWAIT_LEN_LO,
//    STATE_FILL_TYPE,
    STATE_FILL_DATA,
    STATE_CHECK_FILLED
} STATE;

typedef enum {
    UNKNOWN,
    KEYBOARD,
    MOUSE,
    GAMEPAD,
    JOYSTICK
} DEV_TYPE;

typedef struct  {

    uint8_t dpad;
    uint8_t apad;
    uint8_t xpad;

} BUTTONS;

typedef struct {
    uint16_t    r_ptr;
    uint16_t    w_ptr;
    uint16_t    mask;
    uint8_t     data[1024];
} RingBuffer;

typedef struct {
    STATE       state;
    uint8_t     packet[2048];          // I don't know how big this needs to be - size of biggest packet
    uint16_t    packet_ptr;            // If packet array gets bigger, this needs more bits too...
    uint16_t    remain_len;            // Fill length remaining (only valid in STATE_FILL_DATA)
    RingBuffer  *ringBuffer;           // Ringbuffer in use 
} State;

typedef struct {

    uint8_t     length_lo;
    uint8_t     length_hi;
    uint8_t     message_type;
    uint8_t     type;
    uint8_t     device;
    uint8_t     endpoint;
    uint8_t     id_vendor_lo;
    uint8_t     id_vendor_hi;
    uint8_t     id_product_lo;
    uint8_t     id_product_hi;
    uint8_t     payload[2038];

} __attribute__((packed)) FinalPacket;

extern void install_interrupt(CharDevice *device, RingBuffer *uart_a, RingBuffer *uart_b);
extern void remove_interrupt();
extern uint16_t unbuffer(RingBuffer *rb, unsigned char *buffer);

struct KEYB{
    bool pending;
    int raw[6];
    char key[6];
    uint8_t control_keys;
    bool caps;

};

struct GAMEPAD{
    bool pending;
    BUTTONS buttons;
};

//USB HID KEYS - credit to MightyPork: https://gist.github.com/MightyPork/6da26e382a7ad91b5496ee55fdc73db2

#define KEY_MOD_LCTRL  0
#define KEY_MOD_LSHIFT 1
#define KEY_MOD_LALT   2
#define KEY_MOD_LMETA  3
#define KEY_MOD_RCTRL  4
#define KEY_MOD_RSHIFT 5
#define KEY_MOD_RALT   6
#define KEY_MOD_RMETA  7

#define KEY_NONE 0x00 // No key pressed
#define KEY_ERR_OVF 0x01 //  Keyboard Error Roll Over - used for all slots if too many keys are pressed ("Phantom key")
// 0x02 //  Keyboard POST Fail
// 0x03 //  Keyboard Error Undefined
#define KEY_A 0x04 // Keyboard a and A
#define KEY_B 0x05 // Keyboard b and B
#define KEY_C 0x06 // Keyboard c and C
#define KEY_D 0x07 // Keyboard d and D
#define KEY_E 0x08 // Keyboard e and E
#define KEY_F 0x09 // Keyboard f and F
#define KEY_G 0x0a // Keyboard g and G
#define KEY_H 0x0b // Keyboard h and H
#define KEY_I 0x0c // Keyboard i and I
#define KEY_J 0x0d // Keyboard j and J
#define KEY_K 0x0e // Keyboard k and K
#define KEY_L 0x0f // Keyboard l and L
#define KEY_M 0x10 // Keyboard m and M
#define KEY_N 0x11 // Keyboard n and N
#define KEY_O 0x12 // Keyboard o and O
#define KEY_P 0x13 // Keyboard p and P
#define KEY_Q 0x14 // Keyboard q and Q
#define KEY_R 0x15 // Keyboard r and R
#define KEY_S 0x16 // Keyboard s and S
#define KEY_T 0x17 // Keyboard t and T
#define KEY_U 0x18 // Keyboard u and U
#define KEY_V 0x19 // Keyboard v and V
#define KEY_W 0x1a // Keyboard w and W
#define KEY_X 0x1b // Keyboard x and X
#define KEY_Y 0x1c // Keyboard y and Y
#define KEY_Z 0x1d // Keyboard z and Z

#define KEY_1 0x1e // Keyboard 1 and !
#define KEY_2 0x1f // Keyboard 2 and @
#define KEY_3 0x20 // Keyboard 3 and #
#define KEY_4 0x21 // Keyboard 4 and $
#define KEY_5 0x22 // Keyboard 5 and %
#define KEY_6 0x23 // Keyboard 6 and ^
#define KEY_7 0x24 // Keyboard 7 and &
#define KEY_8 0x25 // Keyboard 8 and *
#define KEY_9 0x26 // Keyboard 9 and (
#define KEY_0 0x27 // Keyboard 0 and )

#define KEY_ENTER 0x28 // Keyboard Return (ENTER)
#define KEY_ESC 0x29 // Keyboard ESCAPE
#define KEY_BACKSPACE 0x2a // Keyboard DELETE (Backspace)
#define KEY_TAB 0x2b // Keyboard Tab
#define KEY_SPACE 0x2c // Keyboard Spacebar
#define KEY_MINUS 0x2d // Keyboard - and _
#define KEY_EQUAL 0x2e // Keyboard = and +
#define KEY_LEFTBRACE 0x2f // Keyboard [ and {
#define KEY_RIGHTBRACE 0x30 // Keyboard ] and }
#define KEY_BACKSLASH 0x31 // Keyboard \ and |
#define KEY_HASHTILDE 0x32 // Keyboard Non-US # and ~
#define KEY_SEMICOLON 0x33 // Keyboard ; and :
#define KEY_APOSTROPHE 0x34 // Keyboard ' and "
#define KEY_GRAVE 0x35 // Keyboard ` and ~
#define KEY_COMMA 0x36 // Keyboard , and <
#define KEY_DOT 0x37 // Keyboard . and >
#define KEY_SLASH 0x38 // Keyboard / and ?
#define KEY_CAPSLOCK 0x39 // Keyboard Caps Lock

#define KEY_F1 0x3a // Keyboard F1
#define KEY_F2 0x3b // Keyboard F2
#define KEY_F3 0x3c // Keyboard F3
#define KEY_F4 0x3d // Keyboard F4
#define KEY_F5 0x3e // Keyboard F5
#define KEY_F6 0x3f // Keyboard F6
#define KEY_F7 0x40 // Keyboard F7
#define KEY_F8 0x41 // Keyboard F8
#define KEY_F9 0x42 // Keyboard F9
#define KEY_F10 0x43 // Keyboard F10
#define KEY_F11 0x44 // Keyboard F11
#define KEY_F12 0x45 // Keyboard F12

#define KEY_SYSRQ 0x46 // Keyboard Print Screen
#define KEY_SCROLLLOCK 0x47 // Keyboard Scroll Lock
#define KEY_PAUSE 0x48 // Keyboard Pause
#define KEY_INSERT 0x49 // Keyboard Insert
#define KEY_HOME 0x4a // Keyboard Home
#define KEY_PAGEUP 0x4b // Keyboard Page Up
#define KEY_DELETE 0x4c // Keyboard Delete Forward
#define KEY_END 0x4d // Keyboard End
#define KEY_PAGEDOWN 0x4e // Keyboard Page Down
#define KEY_RIGHT 0x4f // Keyboard Right Arrow
#define KEY_LEFT 0x50 // Keyboard Left Arrow
#define KEY_DOWN 0x51 // Keyboard Down Arrow
#define KEY_UP 0x52 // Keyboard Up Arrow

#define KEY_NUMLOCK 0x53 // Keyboard Num Lock and Clear
#define KEY_KPSLASH 0x54 // Keypad /
#define KEY_KPASTERISK 0x55 // Keypad *
#define KEY_KPMINUS 0x56 // Keypad -
#define KEY_KPPLUS 0x57 // Keypad +
#define KEY_KPENTER 0x58 // Keypad ENTER
#define KEY_KP1 0x59 // Keypad 1 and End
#define KEY_KP2 0x5a // Keypad 2 and Down Arrow
#define KEY_KP3 0x5b // Keypad 3 and PageDn
#define KEY_KP4 0x5c // Keypad 4 and Left Arrow
#define KEY_KP5 0x5d // Keypad 5
#define KEY_KP6 0x5e // Keypad 6 and Right Arrow
#define KEY_KP7 0x5f // Keypad 7 and Home
#define KEY_KP8 0x60 // Keypad 8 and Up Arrow
#define KEY_KP9 0x61 // Keypad 9 and Page Up
#define KEY_KP0 0x62 // Keypad 0 and Insert
#define KEY_KPDOT 0x63 // Keypad . and Delete

#define KEY_102ND 0x64 // Keyboard Non-US \ and |
#define KEY_COMPOSE 0x65 // Keyboard Application
#define KEY_POWER 0x66 // Keyboard Power
#define KEY_KPEQUAL 0x67 // Keypad =

bool isSet(uint8_t value, uint8_t bitindex);

int checkarray(uint8_t val, int * arr, uint8_t arrLen);

void process_strikes(uint8_t* new_keys, uint8_t port);

void process_gamepad(uint8_t* new_pad, uint8_t port, uint16_t vid, uint16_t pid);

void process_final_packet(FinalPacket *p);

void process_raw_packet(uint8_t *raw_packet);

void process_data(uint8_t data, State *state);

void process_incoming(State *state);

//KEYBOARD FUNCTIONS

int kb_pending();

char read_key(State *state);

int read_raw(State *state);

bool check_key(State *state);

struct KEYB get_kb(State *state);

int ugets(State *state, char * buf, int buf_size);

int u_readline(State *state, char *buf, int buf_size);

//GAMEPAD FUNCTIONS

int pad_pending();

BUTTONS read_pad(State *state);

bool check_pad(State *state);

void init_usb();

#endif
