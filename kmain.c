/*
 * Copyright (c) 2022 @HeathenUK, Ring bufffer and state machine implementation by RoscoPeco
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

#define MAX_DEVICES         2

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
    
    bool UP;
    bool UP_RIGHT;
    bool UP_LEFT;
    bool DOWN;
    bool DOWN_RIGHT;
    bool DOWN_LEFT;
    bool LEFT;
    bool RIGHT;

    bool Y;
    bool A;
    bool X;
    bool B;
    
    bool LT;
    bool RT;
    bool LS;
    bool RS;

} PAD[MAX_DEVICES];

struct {
    bool pending;
    char key;
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
         
         for (int i = 2; i < 8; i++) {       // Outer loop - Last keys
            
            if (new_keys[i] == KEY_CAPSLOCK) {
                KB[port].caps = !KB[port].caps;
                return;
            }

            #ifdef DEBUG_PACKETS
            if (new_keys[i] == 0x18) {
                
                printf("\n\nUSB REPORT:\n\n");
                printf("\n\nUSB Endpoint 0:\n");
                printf("Connected? %s\n", USB_DEVICE[0].connected?"Y":"N");
                printf("VID 0x%04x, PID 0x%04x\n", USB_DEVICE[0].vendor_id, USB_DEVICE[0].product_id);
                printf("Type 0x%02x\n", USB_DEVICE[0].dev_type);
                printf("\n");
                printf("\n\nUSB Endpoint 1:\n");
                printf("Connected? %s\n", USB_DEVICE[1].connected?"Y":"N");
                printf("VID 0x%04x, PID 0x%04x\n", USB_DEVICE[1].vendor_id, USB_DEVICE[1].product_id);
                printf("Type 0x%02x\n", USB_DEVICE[1].dev_type);
                printf("\n");

            }
            #endif
            // if (keyboard_cap == false) {
            //     if ((isSet(new_keys[0], 1)) || (isSet(new_keys[0], 5))) {
            //         if (!checkarray(new_keys[i], last_ingest, 8)) fctprintf(mcSendDevice, &duart_a, "%c", keys_upper[new_keys[i]]);
            //     } else {
            //         if (!checkarray(new_keys[i], last_ingest, 8)) fctprintf(mcSendDevice, &duart_a, "%c", keys_lower[new_keys[i]]);
            //     }
            // } else {
            //     if ((isSet(new_keys[0], 1)) || (isSet(new_keys[0], 5))) {
            //         if (!checkarray(new_keys[i], last_ingest, 8)) fctprintf(mcSendDevice, &duart_a, "%c", keys_lower[new_keys[i]]);
            //     } else {
            //         if (!checkarray(new_keys[i], last_ingest, 8)) fctprintf(mcSendDevice, &duart_a, "%c", keys_upper[new_keys[i]]);
            //     }
            // }

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

    //Nvidia Shield Controller Mapper
    if (vid == 0x0955 && pid == 0x7214) {
        uint8_t dpad = new_pad[2];
        uint8_t apad = new_pad[3];

        PAD[port].UP =      (dpad == 0x00) ? true : false;
        PAD[port].RIGHT =   (dpad == 0x02) ? true : false;
        PAD[port].DOWN =    (dpad == 0x04) ? true : false;
        PAD[port].LEFT =    (dpad == 0x06) ? true : false;

        PAD[port].A =       (apad == 0x01) ? true : false;
        PAD[port].B =       (apad == 0x02) ? true : false;
        PAD[port].X =       (apad == 0x04) ? true : false;
        PAD[port].Y =       (apad == 0x08) ? true : false;
        PAD[port].LT =      (apad == 0x10) ? true : false;
        PAD[port].RT =      (apad == 0x20) ? true : false;
        PAD[port].LS =      (apad == 0x40) ? true : false;
        PAD[port].RS =      (apad == 0x80) ? true : false;

        PAD[port].pending = true;
    }
    //DS4 Controller Mapper
    else if ((vid == 0x054c) && ((pid == 0x05CC) || (pid == 0x09CC)) ) {
        uint8_t dpad = new_pad[5] & 0x0F;
        uint8_t apad = new_pad[5] >> 4;

        PAD[port].UP =          (dpad == 0x00) ? true : false;
        PAD[port].UP_RIGHT =    (dpad == 0x01) ? true : false;
        PAD[port].RIGHT =       (dpad == 0x02) ? true : false;
        PAD[port].DOWN_RIGHT =  (dpad == 0x03) ? true : false;
        PAD[port].DOWN =        (dpad == 0x04) ? true : false;
        PAD[port].DOWN_LEFT =   (dpad == 0x05) ? true : false;
        PAD[port].LEFT =        (dpad == 0x06) ? true : false;
        PAD[port].UP_LEFT =     (dpad == 0x07) ? true : false;

        PAD[port].A =           (apad == 0x02) ? true : false;
        PAD[port].B =           (apad == 0x04) ? true : false;
        PAD[port].X =           (apad == 0x01) ? true : false;
        PAD[port].Y =           (apad == 0x08) ? true : false;
        // PAD[port].LT =      (apad == 0x10) ? true : false;
        // PAD[port].RT =      (apad == 0x20) ? true : false;
        // PAD[port].LS =      (apad == 0x40) ? true : false;
        // PAD[port].RS =      (apad == 0x80) ? true : false;

        PAD[port].pending = true;

    }

}

void process_final_packet(FinalPacket *p) {
        
    uint8_t dev = p->endpoint;

    DEBUGX("\nFinal packet of type 0x%02x received from port %d\n", p->message_type, dev);

        if (p->message_type == USB_MSG_CONNECT) {        
            DEBUGX("\nDevice # %d connected, awaiting device info\n",dev);
            USB_DEVICE[dev].connected = true;
        }

        if ((p->message_type == USB_MSG_DESCRIPTOR)) {
            USB_DEVICE[dev].connected = true;
            if (USB_DEVICE[dev].vendor_id == 0xFFFF) {
                USB_DEVICE[dev].vendor_id = p->id_vendor_lo | p->id_vendor_hi << 8;
                USB_DEVICE[dev].product_id = p->id_product_lo | p->id_product_hi << 8;
                DEBUGX("Vendor ID 0x%04x, Product ID 0x%04x connected.\n", USB_DEVICE[dev].vendor_id, USB_DEVICE[dev].product_id);
            }
            
            #ifdef DEBUG_PACKETS
                else DEBUGX("I think I've already announced this device?\n");
            #endif
        }

        else if (p->message_type == USB_MSG_DISCONNECT) {
            
            USB_DEVICE[dev].connected=false;
            USB_DEVICE[dev].vendor_id=0xFFFF;
            USB_DEVICE[dev].product_id=0xFFFF;
            USB_DEVICE[dev].dev_type=UNKNOWN;
            DEBUGX("Device # %d disconnected\n", dev);

        }

        else if (p->message_type == USB_MSG_REPORT) {

            USB_DEVICE[dev].connected = true;
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

            switch (USB_DEVICE[dev].dev_type) {

                    case UNKNOWN: break;
                    case MOUSE: break;
                    case JOYSTICK: break;
                    case KEYBOARD:  process_strikes(&(p->payload[0]), dev); break; 
                    case GAMEPAD:   process_gamepad(&(p->payload[0]), dev, USB_DEVICE[dev].vendor_id, USB_DEVICE[dev].product_id); break;
            }
            

            // if ((p->id_product_lo == 0x14) && (p->id_product_hi == 0x72)) {
            //     process_gamepad(&(p->payload[0]));
            // } else process_strikes(&(p->payload[0]));

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

void kmain() {
    printf("\033*");
    printf("\f");
    if (mcCheckDeviceSupport()) {
        printf("Character device support detected\n");

        uint16_t count = mcGetDeviceCount();
        printf("Found %d device(s)\n", count);

        for (int i = 0; i < count; i++) {
            //CHAR_DEVICE *dev = mcGetDevice(i);
            CharDevice dev;
            if (mcGetDevice(i, &dev)) {

            printf("data    : 0x%08lx\n", dev.data);
            printf("checkptr: 0x%08lx\n", dev.checkptr);
            printf("recvptr : 0x%08lx\n", dev.recvptr);
            printf("sendptr : 0x%08lx\n", dev.sendptr);

            // printf("Printing to device %d\n", i);
            // fctprintf(mcSendDevice, dev, "Hello, World\n"); 
            printf("\n\n");
            }
        }

        printf("Done\n");

        //CHAR_DEVICE *duart_b = mcGetDevice(1);
        CharDevice duart_b;
        mcGetDevice(1, &duart_b);
        install_interrupt(&duart_b);

        printf("Interrupt handler installed\n");
//         //mcDisableInterrupts();
// //        printf("\033*");
//         fctprintf(mcSendDevice, duart_b, "AT\r\n");
// //        delay(500000);
// //        printf("\f");
//         fctprintf(mcSendDevice, duart_b, "AT+RST\r\n");
//         delay(500000);
//         fctprintf(mcSendDevice, duart_b, "AT+GMR\r\n");
//         #ifdef U_DEBUG
//             print("\x1b[8m");
//         #endif
        delay(100000);
//        printf("\f");
        //fctprintf(mcSendDevice, duart_b, "AT+CIPRECVMODE=1\r\n");
        //printf("Going to telnet prompt\n");

        //CHAR_DEVICE *duart_a = mcGetDevice(0);
        CharDevice duart_a;
        if (mcGetDevice(0, &duart_a)) {
        fctprintf(mcSendDevice, &duart_a, "\f");

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
            }

    //Once input has been flagged up, check it, service it, and then return to the main loop.  
    if (PAD[0].pending) {
         PAD[0].pending = false;
         if (PAD[0].UP) printf("0: Up\n");
         else if (PAD[0].DOWN) printf("0: Down\n");
         else if (PAD[0].LEFT) printf("0: Left\n");
         else if (PAD[0].RIGHT) printf("0: Right\n");
    }
    if (PAD[1].pending) {
         PAD[1].pending = false;
         if (PAD[1].UP) printf("1: Up\n");
         else if (PAD[1].DOWN) printf("1: Down\n");
         else if (PAD[1].LEFT) printf("1: Left\n");
         else if (PAD[1].RIGHT) printf("1: Right\n");
    }
      if (KB[0].pending) {
        KB[0].pending = false;
        printf("%c", KB[0].key);
        KB[0].key = 0x00;
    }            
      if (KB[1].pending) {
        KB[1].pending = false;
        printf("%c", KB[1].key);
        KB[1].key = 0x00;
    }

#ifdef DEBUG_HEARTBEAT        
            fctprintf(mcSendDevice, &duart_a, "*");
#endif        
        }
        }
    } else {
        printf("No character device support detected\n");
    }
}

