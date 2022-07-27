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
    STATE   state;
    uint8_t packet[2048];           // I don't know how big this needs to be - size of biggest packet
    uint16_t packet_ptr;            // If packet array gets bigger, this needs more bits too...
    uint16_t remain_len;            // Fill length remaining (only valid in STATE_FILL_DATA)
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

extern void install_interrupt(CharDevice *device);
extern void remove_interrupt();
extern uint16_t unbuffer(unsigned char *buffer);



//static bool keyboard_cap = false;

bool isSet(unsigned value, unsigned bitindex);

int checkarray(uint8_t val, uint8_t* arr, uint8_t arrLen);

void process_strikes(uint8_t* new_keys, uint8_t port);

void process_gamepad(uint8_t* new_pad, uint8_t port, uint16_t vid, uint16_t pid);

void process_final_packet(FinalPacket *p);

void process_raw_packet(uint8_t *raw_packet);

void process_data(uint8_t data, State *state);

void process_incoming(State *state);

//KEYBOARD FUNCTIONS

int kb_pending();

char read_key(State *state);

bool check_key(State *state);

int u_readline(State *state, char *buf, int buf_size);

char* ugets(State *state, char *buf, int n);

//GAMEPAD FUNCTIONS

int pad_pending();

BUTTONS read_pad(State *state);

bool check_pad(State *state);

void init_usb();

#endif