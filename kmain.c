/*
 * Copyright (c) 2022 @HeathenUK, ring bufffer and state machine implementation by RoscoPeco
 */

#include <stdio.h>
#include <machine.h>
#include <basicio.h>
#include <printf.h>
#include <string.h>

//#define U_DEBUG
//#define DEBUG_PACKETS           // Define to dump some debug info
//#define DEBUG_VERBOSE           // Define to dump more debug info (use along with DEBUG_PACKETS)
//#define DEBUG_HEARTBEAT         // Define to print the heartbeat asterisks

#ifdef DEBUG_PACKETS
//#define DEBUGF(...) do { printf(__VA_ARGS__); } while (0)
#define DEBUGF(...) do { CharDevice duart_a; if (mcGetDevice(0, &duart_a)) { fctprintf(mcSendDevice, &duart_a, __VA_ARGS__); }} while (0)
#else
#define DEBUGF(...)
#endif

#ifdef DEBUG_PACKETS
//#define DEBUGF(...) do { printf(__VA_ARGS__); } while (0)
#define DEBUGX(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DEBUGX(...)
#endif

#define KEY_CAPSLOCK 0x39

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

struct {
    bool pending;
    uint8_t dpad;
    uint8_t apad;
    uint8_t xpad;     
    // bool UP;
    // bool UP_RIGHT;
    // bool UP_LEFT;
    // bool DOWN;
    // bool DOWN_RIGHT;
    // bool DOWN_LEFT;
    // bool LEFT;
    // bool RIGHT;
    
    // bool D_DEAD;
    // bool B_DEAD;

    // bool Y;
    // bool A;
    // bool X;
    // bool B;
    
    // bool LT;
    // bool RT;

    // bool L2;
    // bool R2;

    // bool LS;
    // bool RS;

    // bool START;
    // bool SELECT;

} PAD[MAX_DEVICES];

struct {
    bool pending;
    char key;
    char key2;
    char key3;
    char key4;
    char key5;
    char key6;
    uint8_t control_keys;
    bool caps;

} KB[MAX_DEVICES];

struct {
    bool        connected;
    uint16_t    vendor_id;
    uint16_t    product_id;
    DEV_TYPE    dev_type;

} USB_DEVICE[MAX_DEVICES];

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

unsigned char keys_upper[60] = {0x00,'\0','\0','\0',
'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
'!','"',0xA3,'$','%','^','&','*','(',')',
'\n','\0','\0',' ', ' ','_','+','{','}','|','~',':',0x40,0xAC,'<','>','?','\0'}; 

unsigned char keys_lower[60] = {0x00,'\0','\0','\0',
'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
'1','2','3','4','5','6','7','8','9','0',
'\n','\0','\0',' ', ' ','-','=','[',']','\\','#',';','\'','`',',','.','/','\0'}; 

uint8_t last_ingest[8] = {'\0','\0','\0','\0','\0','\0','\0','\0',};

//static bool keyboard_cap = false;

bool isSet(unsigned value, unsigned bitindex)
{
    return (value & (1 << bitindex)) != 0;
}

int checkarray(uint8_t val, uint8_t* arr, uint8_t arrLen)
{

    for (int i = 0; i < arrLen; i++) {
        if (arr[i] == val) {
            return 1;
        }
    }

    return 0;

}

void process_strikes(uint8_t* new_keys, uint8_t port) {
         
    KB[port].control_keys = new_keys[0];
    for (int i = 2; i < 8; i++) {       // Outer loop - Last keys
        
        if (new_keys[i] == KEY_CAPSLOCK) {
            KB[port].caps = !KB[port].caps;
            return;
        }

        if ((KB[port].caps) || (isSet(new_keys[0], 1)) || (isSet(new_keys[0], 5))) {
            if (!checkarray(new_keys[i], last_ingest, 8)) KB[port].key = keys_upper[new_keys[i]];
        } else {
            if (!checkarray(new_keys[i], last_ingest, 8)) KB[port].key = keys_lower[new_keys[i]];
        }
    }
    //printf("%c registered\n", KB[port].key);
    KB[port].pending = true;
    memcpy(last_ingest, new_keys, 8);

}

void process_gamepad(uint8_t* new_pad, uint8_t port, uint16_t vid, uint16_t pid) {

    //D-Pad byte should always follow HAT order:
        //UP = 0
        //UP_RIGHT = 1
        //RIGHT = 2
        //DOWN_RIGHT = 3
        //DOWN = 4
        //DOWN_LEFT = 5
        //LEFT = 6
        //UP_LEFT = 7
        //DEAD = 8
    
    //A-Pad byte should be mapped to follow:
        //A = 1st bit
        //B = 2nd bit
        //X = 3rd bit
        //Y = 4th bit
        //L1 = 5th bit
        //L2 = 6th bit
        //Start = 7th bit
        //Select = 8th bit

    //X-Pad byte should be mapped to follow:
        //L2 = 1st bit
        //R2 = 2nd bit
        //LS = 3rd bit
        //RS = 4th bit
        //TBC = 5th bit
        //TBC = 6th bit
        //TBC = 7th bit
        //TBC = 8th bit

    //Nvidia Shield Controller Mapper
    if (vid == 0x0955 && pid == 0x7214) {

        PAD[port].dpad = new_pad[2]; //This pad respects HAT order, so take nibble directly.
        PAD[port].apad = new_pad[3]; //Almost compliant, we will clobber LS and RS (bits 7 and 8) later.
        PAD[port].xpad = 0;
            
//            PAD[port].LS =      (apad == 0x40) ? true : false;
//            PAD[port].RS =      (apad == 0x80) ? true : false;

        PAD[port].pending = true;
    }
    //DS4 mapper
    else if ((vid == 0x054c) && ((pid == 0x05CC) || (pid == 0x09CC)) ) {
        uint8_t dpad = new_pad[5] & 0x0F;
        uint8_t apad = new_pad[5] >> 4;
        uint8_t xpad = new_pad[6];

        PAD[port].dpad = dpad; //This pad respects HAT order, so take nibble directly.
        PAD[port].apad = 0; //Default to dead
        PAD[port].xpad = 0; //Default to dead

        if (apad != 0x00) {
            if ((apad >> 0) & 1) PAD[port].apad |= (1 << 2); //If 1st bit of apad is set, is X (Square), goes in 3rd bit of APAD
            if ((apad >> 1) & 1) PAD[port].apad |= (1 << 0); //If 2nd bit is set, is A (Cross), goes in 1st bit of APAD
            if ((apad >> 2) & 1) PAD[port].apad |= (1 << 1); //If 3rd bit is set, is B (Circle), goes in 2nd bit of APAD
            if ((apad >> 3) & 1) PAD[port].apad |= (1 << 3); //If 4th bit is set, is Y (Triangle), goes in 4th bit of APAD

        }

        if (xpad != 0x00) {
            if ((xpad >> 0) & 1) PAD[port].apad |= (1 << 4); //If 1st bit of xpad is set, is L1, goes in 5th bit of APAD
            if ((xpad >> 2) & 1) PAD[port].apad |= (1 << 5); //If 3rd bit is set, is L2, goes in 6th bit of APAD
            if ((xpad >> 5) & 1) PAD[port].apad |= (1 << 6); //If 5th bit is set, is START, goes in 7th bit of APAD
            if ((xpad >> 4) & 1) PAD[port].apad |= (1 << 7); //If 4th bit is set, is SELECT, goes in 8th bit of APAD
            
            if ((xpad >> 2) & 1) PAD[port].xpad |= (1 << 0); //If 3rd bit of xpad is set, is L2, goes in 1st bit of XPAD
            if ((xpad >> 3) & 1) PAD[port].xpad |= (1 << 1); //If 4th bit is set, is R2, goes in 2nd bit of XPAD
            if ((xpad >> 6) & 1) PAD[port].xpad |= (1 << 2); //If 7th bit is set, is LS, goes in 3rd bit of XPAD
            if ((xpad >> 7) & 1) PAD[port].xpad |= (1 << 3); //If 8th bit is set, is RS, goes in 4th bit of XPAD
        }

        PAD[port].pending = true;
    }  

}

void process_final_packet(FinalPacket *p) {
        
    uint8_t dev = p->device;

    DEBUGX("\nFinal packet of type 0x%02x received from device %d\n", p->message_type, dev);

        // if (p->message_type == USB_MSG_CONNECT) {        
        //     DEBUGX("\nDevice # %d connected, awaiting device info\n",dev);
        //     USB_DEVICE[dev].connected = true;
        // }

        // if ((p->message_type == USB_MSG_DESCRIPTOR)) {
        //     USB_DEVICE[dev].connected = true;
        //     if (USB_DEVICE[dev].vendor_id == 0xFFFF) {
        //         USB_DEVICE[dev].vendor_id = p->id_vendor_lo | p->id_vendor_hi << 8;
        //         USB_DEVICE[dev].product_id = p->id_product_lo | p->id_product_hi << 8;
        //         DEBUGX("Vendor ID 0x%04x, Product ID 0x%04x connected.\n", USB_DEVICE[dev].vendor_id, USB_DEVICE[dev].product_id);
        //     }
            
        //     #ifdef DEBUG_PACKETS
        //         else DEBUGX("I think I've already announced this device?\n");
        //     #endif
        // }

        // else if (p->message_type == USB_MSG_DISCONNECT) {
            
        //     USB_DEVICE[dev].connected=false;
        //     USB_DEVICE[dev].vendor_id=0xFFFF;
        //     USB_DEVICE[dev].product_id=0xFFFF;
        //     USB_DEVICE[dev].dev_type=UNKNOWN;
        //     DEBUGX("Device # %d disconnected\n", dev);

        // }

        if (p->message_type == USB_MSG_REPORT) {

            if (USB_DEVICE[dev].vendor_id == 0xFFFF) {
                USB_DEVICE[dev].vendor_id = p->id_vendor_lo | p->id_vendor_hi << 8;
                USB_DEVICE[dev].product_id = p->id_product_lo | p->id_product_hi << 8;
                DEBUGX("Vendor ID 0x%04x, Product ID 0x%04x connected.\n", USB_DEVICE[dev].vendor_id, USB_DEVICE[dev].product_id);
            }

            if (USB_DEVICE[dev].dev_type == UNKNOWN) {
                
                switch (p->type) {

                    case TYPE_GAMEPAD: USB_DEVICE[dev].dev_type =  GAMEPAD; break;
                    case TYPE_MOUSE: USB_DEVICE[dev].dev_type =    MOUSE; break;
                    case TYPE_KEYBOARD: USB_DEVICE[dev].dev_type = KEYBOARD; break;
                    case TYPE_JOYSTICK: USB_DEVICE[dev].dev_type = JOYSTICK; break;

                }

            }
            
        }

        switch (USB_DEVICE[dev].dev_type) {

                case UNKNOWN: break;
                case MOUSE: break;
                case JOYSTICK: break;
                case KEYBOARD:  process_strikes(&(p->payload[0]), dev); break; 
                case GAMEPAD:   process_gamepad(&(p->payload[0]), dev, USB_DEVICE[dev].vendor_id, USB_DEVICE[dev].product_id); break;
        }

}



void process_raw_packet(uint8_t *raw_packet) {        
    process_final_packet((FinalPacket*)raw_packet);
}

void process_data(uint8_t data, State *state) {
    
    switch (state->state) {
    case STATE_DISCARD:
        if (data == 0xFE) {
            DEBUGF("(STATE_DISCARD)         -> STATE_AWAIT_SIG_1 [0x%02x]\r\n\r\n", data);
            state->state = STATE_AWAIT_SIG_1;
        }
        break;

    case STATE_AWAIT_SIG_1: 
        if (data == 0xED) {
            // valid - length is next
            DEBUGF("(STATE_AWAIT_SIG_1      -> STATE_AWAIT_LEN_HI [0x%02x]\r\n\r\n", data);
            state->state = STATE_AWAIT_LEN_LO;
            state->packet_ptr = 0;
        } else {
            // invalid - back to discard
            DEBUGF("(STATE_AWAIT_SIG_1      -> STATE_DISCARD [0x%02x]\r\n\r\n", data);
            state->state = STATE_DISCARD;
        }

        break;

    case STATE_AWAIT_LEN_LO:
        
        DEBUGF("(STATE_AWAIT_LEN_LO     -> STATE_AWAIT_LEN_HI [0x%02x]\r\n\r\n", data);
        state->remain_len = data;
        state->state = STATE_AWAIT_LEN_HI;
        state->packet[state->packet_ptr++] = data;
        break;

    case STATE_AWAIT_LEN_HI:    
        
        DEBUGF("(STATE_AWAIT_LEN_HI     -> STATE_FILL_DATA [0x%02x]\r\n\r\n", data);
        
        if (data == 0x0A) {

            state->state = STATE_DISCARD;
            break;

        }
        
        state->remain_len |= data << 8;
        state->remain_len += 8;
        state->state = STATE_FILL_DATA;
        state->packet[state->packet_ptr++] = data;
        break;

    case STATE_FILL_DATA:

        state->packet[state->packet_ptr++] = data;

        if (--state->remain_len == 0) {
            // All data transferred, expect a "\n" to close the communication.
            DEBUGF("(STATE_FILL_DATA         -> STATE_CHECK_FILLED (and process) [0x%02x] [remain_len = 0x%04x]\r\n\r\n", data, state->remain_len);
            state->state = STATE_CHECK_FILLED;
            
        } 
#ifdef DEBUG_PACKETS
        else {
            DEBUGF("                        #  FILL [0x%02x] [remain_len = 0x%04x]\r\n\r\n", data, state->remain_len);
        }
#endif

        break;

   case STATE_CHECK_FILLED:
        if (data != '\n') {
            DEBUGF("GOT 0x%02x. BAD PACKET?\r\n", data);
        }

        state->state = STATE_DISCARD;
        process_raw_packet(state->packet);

        break;
        
    }
}

void process_incoming(State *state) {
    static uint8_t buffer[1024];
#ifdef DEBUG_PACKETS
#ifdef DEBUG_VERBOSE
    //CHAR_DEVICE *duart_a = mcGetDevice(0);
    CharDevice duart_a;
    if (mcGetDevice(0, &duart_a)) {
    #endif
    #endif

        uint16_t count = unbuffer(buffer);      //Pull the ring buffer
        if (count == 0) return;

    #ifdef DEBUG_PACKETS
    #ifdef DEBUG_VERBOSE
        for (int i = 0; i < count; i++) {
            fctprintf(mcSendDevice, &duart_a, "0x%02x ", buffer[i]);
        }
        fctprintf(mcSendDevice, &duart_a, "\r\n\r\n");
    }
    #endif
    #endif

        for (int i = 0; i < count; i++) {
            process_data(buffer[i], state);
        }
}

//KEYBOARD FUNCTIONS

uint8_t kb_pending() {

    int test = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {

        if (KB[i].pending) test = i;

    }

    //return the first KB-type device with a character pending.
    return test;

}

char read_key(State *state) {

    while (!kb_pending()) {
        process_incoming(&state);
    }
    
    uint8_t kb = kb_pending();
    KB[kb].pending = false;
    char ret_key = KB[kb].key;
    KB[kb].key = 0x00;
    return ret_key;

}

bool check_key(State *state) {

    process_incoming(&state);
    if (kb_pending()) return true;
    else return false;

}

//GAMEPAD FUNCTIONS

uint8_t pad_pending() {

    int test = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {

        if (PAD[i].pending) test = i;

    }

    //return the first PAD-type device with a character pending.
    return test;

}

uint8_t read_dpad(State *state) {

    while (pad_pending()) {
        process_incoming(&state);
    }
    
    uint8_t pad = pad_pending();
    return PAD[pad].dpad;

}

uint8_t read_apad(State *state) {

    while (pad_pending()) {
        process_incoming(&state);
    }
    
    uint8_t pad = pad_pending();
    return PAD[pad].apad;

}

uint8_t read_xpad(State *state) {

    while (pad_pending()) {
        process_incoming(&state);
    }
    
    uint8_t pad = pad_pending();
    return PAD[pad].xpad;

}

bool check_pad(State *state) {

    process_incoming(&state);
    if (pad_pending()) return true;
    else return false;

}

void kmain() {
    printf("\033*");
    printf("\f");

    CharDevice duart_b;
    mcGetDevice(1, &duart_b);
    install_interrupt(&duart_b);

    CharDevice duart_a;
    if (mcGetDevice(0, &duart_a)) fctprintf(mcSendDevice, &duart_a, "\f");

    State state;
    state.state = STATE_DISCARD;

    for (int i = 0; i < MAX_DEVICES; i++) {

        USB_DEVICE[i].connected=false;
        USB_DEVICE[i].vendor_id=0xFFFF;
        USB_DEVICE[i].product_id=0xFFFF;
        USB_DEVICE[i].dev_type=UNKNOWN;

    }

    while (true) {            
        
        //Main loop - go until input is provided (do other things in here)
        while ((!PAD[0].pending) && (!PAD[1].pending && (!KB[0].pending)) && (!KB[1].pending)) {
            process_incoming(&state);
            #ifdef DEBUG_HEARTBEAT        
                if (mcGetDevice(0, &duart_a)) fctprintf(mcSendDevice, &duart_a, "*");
            #endif
        }

    //Once input has been flagged up, check it, service it, and then return to the main loop.  
    for (i == 0; i < MAX_DEVICES; i++) {
        // if (PAD[i].pending) {
        //     PAD[i].pending = false;
        //     if (!PAD[i].D_DEAD) {
        //         if         (PAD[i].UP) printf("%d: Up\n", i);
        //         else if    (PAD[i].UP_RIGHT) printf("%d: Up-right\n", i);
        //         else if    (PAD[i].UP_LEFT) printf("%d: Up-left\n", i);
        //         else if    (PAD[i].DOWN) printf("%d: Down\n", i);
        //         else if    (PAD[i].DOWN_RIGHT) printf("%d: Down-right\n", i);
        //         else if    (PAD[i].DOWN_LEFT) printf("%d: Down-left\n", i);
        //         else if    (PAD[i].LEFT) printf("%d: Left\n", i);
        //         else if    (PAD[i].RIGHT) printf("%d: Right\n", i);
        //     } 
        //     if (!PAD[i].B_DEAD) {
        //         if         (PAD[i].A) printf("%d: A\n", i);
        //         else if    (PAD[i].B) printf("%d: B\n", i);
        //         else if    (PAD[i].X) printf("%d: X\n", i);
        //         else if    (PAD[i].Y) printf("%d: Y\n", i);
                        
        //         else if    (PAD[i].LT) printf("%d: Left Trigger\n", i);
        //         else if    (PAD[i].L2) printf("%d: Left Trigger 2\n", i);
        //         else if    (PAD[i].RT) printf("%d: Right Trigger\n", i);
        //         else if    (PAD[i].R2) printf("%d: Right Trigger 2\n", i);

        //         else if    (PAD[i].LS) printf("%d: Left Stick\n", i);
        //         else if    (PAD[i].RS) printf("%d: Right Stick\n", i);

        //         else if    (PAD[i].START) printf("%d: Start\n", i);
        //         else if    (PAD[i].SELECT) printf("%d: Select\n", i);
        //     }
        // }
        
        if (KB[i].pending) {
            KB[i].pending = false;
            printf("%c", KB[i].key);
            KB[i].key = 0x00;
        }            
        
        } 
    }
}

