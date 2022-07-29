#include <stdio.h>
#include <machine.h>
#include <basicio.h>
#include <printf.h>
#include <string.h>
#include "ROSCO_CH559.h"

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


struct GAMEPAD PAD[MAX_DEVICES];

struct KEYB KB[MAX_DEVICES];

struct {
    bool        connected;
    uint16_t    vendor_id;
    uint16_t    product_id;
    DEV_TYPE    dev_type;

} USB_DEVICE[MAX_DEVICES];

unsigned char keys_upper[100] = {0x00,0x1C,0x1C,0x1C,                                       //0x00 - 0x03
'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O',                                //0x04 - 0x12
'P','Q','R','S','T','U','V','W','X','Y','Z',                                                //0x13 - 0x1D
'!','"',0xA3,'$','%','^','&','*','(',')',                                                   //0x1E - 0x27
'\r',0x1C,0x08,' ', ' ','_','+','{','}','|','~',':',                                        //0x28 - 0x33
0x40,0xAC,'<','>','?',0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,     //0x34 - 0x45 (F12)
0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x7F};                                                        //0x46 - 0x4C (DELETE)

unsigned char keys_lower[100] = {0x00,0x1C,0x1C,0x1C,
'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o',
'p','q','r','s','t','u','v','w','x','y','z',
'1','2','3','4','5','6','7','8','9','0',
'\r',0x1C,0x08,' ', ' ','-','=','[',']','\\','#',';',
'\'','`',',','.','/',0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,
0x1C,0x1C,0x1C,0x1C,0x1C,0x1C,0x7F};    

unsigned char last_ingest[8] = {'\0','\0','\0','\0','\0','\0','\0','\0'};


bool isSet(uint8_t value, uint8_t bitindex)
{
    return (value & (1 << bitindex)) != 0;
}

int checkarray(uint8_t val, int * arr, uint8_t arrLen)
{

    for (int i = 0; i < arrLen; i++) {
        if (arr[i] == val) {
            return 1;
        }
    }

    return 0;

}

void process_strikes(uint8_t* new_keys, uint8_t port) {

    if (new_keys[0]) {                      //First byte from keyboard = control keys (shift, ctrl etc.)
        KB[port].control_keys = new_keys[0]; 
        KB[port].pending = true;
    }

    for (int i = 2; i < 8; i++) {           //Final six bytes (second byte is reserved) are USB keycodes.

        if (new_keys[i] == KEY_CAPSLOCK) {  //Special case for caps lock, store for logic below but do nothing else with it.
            KB[port].caps = !KB[port].caps;
            return;
        }

        if ((KB[port].caps) || (isSet(new_keys[0], 1)) || (isSet(new_keys[0], 5))) {
            //if (!checkarray(new_keys[i], last_ingest, 8)) KB[port].key = keys_upper[new_keys[i]];                 
            if (!checkarray(new_keys[i], &KB[port].raw[i - 2], 6)) {                                      //If the new raw code doesn't feature in the array of old raw codes
                KB[port].key[i - 2] = keys_upper[new_keys[i]];                                           //then this has been pressed and released, store the ASCII equivalent.
                KB[port].pending = true;
            }
        } else {
            //if (!checkarray(new_keys[i], last_ingest, 8)) KB[port].key = keys_lower[new_keys[i]];
            if (!checkarray(new_keys[i], &KB[port].raw[i - 2], 6)) {
                KB[port].key[i - 2] = keys_lower[new_keys[i]];
                KB[port].pending = true;
            }
        }
        KB[port].raw[i - 2] = new_keys[i];  //Store the raw USB keycodes (to allow processing non-ascii keys like arrows etc.)
                                            //and, since this should also store zeroes, for comparison against next loop.
    }
    //printf("%c registered\n", KB[port].key);
    //memcpy(last_ingest, new_keys, 8);

}

void remap_into_dpad(uint8_t port, uint8_t map, uint8_t from, uint8_t to) {

    if (isSet(map, from)) {
        PAD[port].buttons.dpad |= (1 << to);
    }

}

void remap_into_apad(uint8_t port, uint8_t map, uint8_t from, uint8_t to) {

    if (isSet(map, from)) PAD[port].buttons.apad |= (1 << to);

}

void remap_into_xpad(uint8_t port, uint8_t map, uint8_t from, uint8_t to) {

    if (isSet(map, from)) PAD[port].buttons.xpad |= (1 << to);

}

void process_gamepad(uint8_t* new_pad, uint8_t port, uint16_t vid, uint16_t pid) {

    PAD[port].buttons.dpad = 0;
    PAD[port].buttons.apad = 0;
    PAD[port].buttons.xpad = 0;

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

        PAD[port].buttons.dpad = new_pad[2];; //This almost pad respects HAT order, so take nibble directly to start with.
        
        remap_into_dpad(port, PAD[port].buttons.dpad, 7, 3); //Outlier is that "dead" is signalled by bit 8 rather than bit 4, so use that bit and then clear the leftover.
        PAD[port].buttons.dpad &= ~(1 << 7);
        
        PAD[port].buttons.apad = new_pad[3]; //Almost compliant, we will clobber LS and RS (bits 7 and 8) later.
        PAD[port].buttons.xpad = 0;
            
//            PAD[port].LS =      (apad == 0x40) ? true : false;
//            PAD[port].RS =      (apad == 0x80) ? true : false;

        PAD[port].pending = true;
    }
    //DS4 mapper
    else if ((vid == 0x054c) && ((pid == 0x05CC) || (pid == 0x09CC)) ) {
        uint8_t dpad = new_pad[5] & 0x0F;
        uint8_t apad = new_pad[5] >> 4;
        uint8_t xpad = new_pad[6];

        PAD[port].buttons.dpad = dpad; //This pad respects HAT order, so take nibble directly.
        PAD[port].buttons.apad = 0; //Default to dead
        PAD[port].buttons.xpad = 0; //Default to dead

        if (apad != 0x00) {
            remap_into_apad(port, apad, 0, 2);
            remap_into_apad(port, apad, 1, 0);
            remap_into_apad(port, apad, 2, 1);
            remap_into_apad(port, apad, 3, 3);
            // if (isSet(apad, 0)) PAD[port].buttons.apad |= (1 << 2); //If 1st bit of apad is set, is X (Square), goes in 3rd bit of APAD
            // if (isSet(apad, 1)) PAD[port].buttons.apad |= (1 << 0); //If 2nd bit is set, is A (Cross), goes in 1st bit of APAD
            // if (isSet(apad, 2)) PAD[port].buttons.apad |= (1 << 1); //If 3rd bit is set, is B (Circle), goes in 2nd bit of APAD
            // if (isSet(apad, 3)) PAD[port].buttons.apad |= (1 << 3); //If 4th bit is set, is Y (Triangle), goes in 4th bit of APAD

        }

        if (xpad != 0x00) {
            remap_into_apad(port, xpad, 0, 4);
            remap_into_apad(port, xpad, 1, 5);
            remap_into_xpad(port, xpad,  2, 0);
            remap_into_xpad(port, xpad,  3, 1);
            remap_into_apad(port, xpad,  4, 7);
            remap_into_apad(port, xpad,  5, 6);
            remap_into_xpad(port, xpad,  6, 2);
            remap_into_xpad(port, xpad,  7, 3);
            // if (isSet(xpad, 0)) PAD[port].buttons.apad |= (1 << 4); //If 1st bit of xpad is set, is L1, goes in 5th bit of APAD
            // if (isSet(xpad, 1)) PAD[port].buttons.apad |= (1 << 5); //If 2nd bit is set, is R1, goes in 6th bit of APAD
            // if (isSet(xpad, 2)) PAD[port].buttons.xpad |= (1 << 0); //If 3rd bit of xpad is set, is L2, goes in 1st bit of XPAD
            // if (isSet(xpad, 3)) PAD[port].buttons.xpad |= (1 << 1); //If 4th bit is set, is R2, goes in 2nd bit of XPAD
            // if (isSet(xpad, 4)) PAD[port].buttons.apad |= (1 << 7); //If 5th bit is set, is SELECT, goes in 8th bit of APAD
            // if (isSet(xpad, 5)) PAD[port].buttons.apad |= (1 << 6); //If 6th bit is set, is START, goes in 7th bit of APAD
            // if (isSet(xpad, 6)) PAD[port].buttons.xpad |= (1 << 2); //If 7th bit is set, is LS, goes in 3rd bit of XPAD
            // if (isSet(xpad, 7)) PAD[port].buttons.xpad |= (1 << 3); //If 8th bit is set, is RS, goes in 4th bit of XPAD
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


            USB_DEVICE[dev].vendor_id = p->id_vendor_lo | p->id_vendor_hi << 8;
            USB_DEVICE[dev].product_id = p->id_product_lo | p->id_product_hi << 8;
            DEBUGX("Vendor ID 0x%04x, Product ID 0x%04x connected.\n", USB_DEVICE[dev].vendor_id, USB_DEVICE[dev].product_id);
            

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

        uint16_t count = unbuffer(state->ringBuffer, buffer);      //Pull the ring buffer
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

int kb_pending() {

    int test = -1;
    for (int i = 0; i < MAX_DEVICES; i++) {

        if (KB[i].pending) test = i;

    }

    //return the first KB-type device with a character pending.
    return test;

}

char read_key(State *state) {
    
    while (kb_pending() == -1) {
        process_incoming(state);
    }
    uint8_t kb = kb_pending();
    KB[kb].pending = false;
    char ret_key = KB[kb].key[0];
    KB[kb].key[0] = 0x00;
    return ret_key;

}

int read_raw(State *state) {

    while (kb_pending() == -1) {
        process_incoming(state);
    }
    uint8_t kb = kb_pending();
    KB[kb].pending = false;
    char ret_key = KB[kb].raw[0];
    KB[kb].raw[0] = 0x00;
    return ret_key;

}

bool check_key(State *state) {

    process_incoming(state);
    if (kb_pending()) return true;
    else return false;

}

struct KEYB get_kb(State *state) {

    struct KEYB ret_kb;

    while (kb_pending() == -1) {
        process_incoming(state);
    }
    
    uint8_t kb = kb_pending();
    
    KB[kb].pending = false;
    ret_kb = KB[kb];

    return ret_kb;

}

int ugets(State *state, char * buf, int buf_size) //Credit to Xark for original version
{
    //ctrl_c_flag = false;
    memset(buf, 0, buf_size);

    int len = 0;
    while (true)
    {
        char c = read_key(state);

        // accept string
        if (c == '\r')
        {
            break;
        }

        switch (c)
        {
            // backspace
            case '\b': /* ^H */
            case 0x7F: /* DEL */
                if (len > 0)
                {
                    buf[--len] = '\0';
                    printf("\b \b");
                }
                break;
            // clear string
            case '\x3':  /* ^C */
            case '\x18': /* ^X */
                while (len > 0)
                {
                    buf[--len] = '\0';
                    printf("\b \b");
                }
                break;

            // add non-control character
            default:
                if (len < (buf_size - 1) && c >= ' ')
                {
                    printchar(c);
                    buf[len++] = c;
                }
        }
    }
    printf("\n");
    // make sure string is terminated
    buf[len] = 0;
    return len;
}

//GAMEPAD FUNCTIONS

int pad_pending() {

    int test = -1;
    for (int i = 0; i < MAX_DEVICES; i++) {

        if (PAD[i].pending) test = i;

    }
    return test;
    //return the first PAD-type device with a character pending.

}

BUTTONS read_pad(State *state) {

    while (pad_pending() == -1) {
        process_incoming(state);
    }
    
    uint8_t pad = pad_pending();
    PAD[pad].pending = false;
    return PAD[pad].buttons;

}

bool check_pad(State *state) {

    process_incoming(state);
    if (pad_pending()) return true;
    else return false;

}

void init_usb() {

    for (int i = 0; i < MAX_DEVICES; i++) {

        USB_DEVICE[i].connected=false;
        KB[i].pending = false;
        PAD[i].pending = false;
        USB_DEVICE[i].vendor_id=0xFFFF;
        USB_DEVICE[i].product_id=0xFFFF;
        USB_DEVICE[i].dev_type=UNKNOWN;

    }

}
