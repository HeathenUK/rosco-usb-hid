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

bool isSet(unsigned value, unsigned bitindex)
{
    return (value & (1 << bitindex)) != 0;
}

// uint8_t setBit(uint8_t *value, uint8_t bitindex)
// {
//     value |= 1 << bitindex;
//     return value;

// }

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

void remap_dpad(uint8_t port, uint8_t from, uint8_t to) {

    if (isSet(dpad, from)) PAD[port].buttons.dpad |= (1 << to);

}

void remap_apad(uint8_t port, uint8_t from, uint8_t to) {

    if (isSet(apad, from)) PAD[port].buttons.apad |= (1 << to);

}

void remap_xpad(uint8_t port, uint8_t from, uint8_t to) {

    if (isSet(xpad, from)) PAD[port].buttons.xpad |= (1 << to);

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

        PAD[port].buttons.dpad = new_pad[2] & 0x0F; //This pad respects HAT order, so take nibble directly.
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
            remap_apad(port, 0, 2);
            remap_apad(port, 1, 0);
            remap_apad(port, 2, 1);
            remap_apad(port, 3, 3);
            // if (isSet(apad, 0)) PAD[port].buttons.apad |= (1 << 2); //If 1st bit of apad is set, is X (Square), goes in 3rd bit of APAD
            // if (isSet(apad, 1)) PAD[port].buttons.apad |= (1 << 0); //If 2nd bit is set, is A (Cross), goes in 1st bit of APAD
            // if (isSet(apad, 2)) PAD[port].buttons.apad |= (1 << 1); //If 3rd bit is set, is B (Circle), goes in 2nd bit of APAD
            // if (isSet(apad, 3)) PAD[port].buttons.apad |= (1 << 3); //If 4th bit is set, is Y (Triangle), goes in 4th bit of APAD

        }

        if (xpad != 0x00) {
            remap_xpad(port, 0, 4);
            remap_xpad(port, 1, 5);
            remap_xpad(port, 2, 0);
            remap_xpad(port, 3, 1);
            remap_xpad(port, 4, 7);
            remap_xpad(port, 5, 6);
            remap_xpad(port, 6, 2);
            remap_xpad(port, 7, 3);
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
    char ret_key = KB[kb].key;
    KB[kb].key = 0x00;
    return ret_key;

}

bool check_key(State *state) {

    process_incoming(state);
    if (kb_pending()) return true;
    else return false;

}

int u_readline(char *buf, int buf_size) { // WIP - Probably wouldn't compile right now
  register char c;
  register uint8_t i = 0;

  while (i < buf_size - 1) {
    c = buf[i] = read_key();

    switch (c) {
    case 0x08:
    case 0x7F:  /* DEL */
      if (i > 0) {
        buf[i-1] = 0;
        i = i - 1;
        printf(0x08);
      }
      break;
    case 0x0A:
      // throw this away...
      break;
    case 0x0D:
      // return
      buf[i] = 0;
      printf("\n");
      return i;
    default:
      buf[i++] = c;
      sendbuf[0] = c;
      printf(sendbuf);
    }
  }

  buf[buf_size-1] = 0;
  return buf_size;
}

char* ugets(char *buf, int n, FILE *stream) {
  int len = u_readline(buf, n);

  if (len > 0 && len < (n - 1)) {
    buf[len] = '\n';
    buf[len+1] = 0;
  }

  return buf;
}

// const char* ugets(State *state, uint16_t max_size) { //Working pre u_readline code.

//     static char ugets_buffer[1024] = "";
//     memset(ugets_buffer,0,sizeof(ugets_buffer));
//     while (true) {
//         char new_key = read_key(state);
//         strncat(ugets_buffer, &new_key, 1);
//         if ((new_key == '\n') || (strlen(ugets_buffer) == (long unsigned int)(max_size - 1))) break;
//     }

//     return ugets_buffer;

// }

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