/*
 * Copyright (c) 2022 @HeathenUK
 */

#include <stdio.h>
#include <machine.h>
#include <basicio.h>
#include <printf.h>
#include <string.h>

//#define U_DEBUG
#define DEBUG_PACKETS           // Define to dump some debug info
//#define DEBUG_VERBOSE           // Define to dump more debug info (use along with DEBUG_PACKETS)
//#define DEBUG_HEARTBEAT         // Define to print the heartbeat asterisks

#ifdef DEBUG_PACKETS
//#define DEBUGF(...) do { printf(__VA_ARGS__); } while (0)
#define DEBUGF(...) do { CHAR_DEVICE *duart_a = mcGetDevice(0); fctprintf(mcSendDevice, duart_a, __VA_ARGS__); } while (0)
#else
#define DEBUGF(...)
#endif


#define USB_MSG_CONNECT     0x01
#define USB_MSG_DISCONNECT  0x02
#define USB_MSG_ERROR       0x03
#define USB_MSG_REPORT      0x04

typedef struct {
    uint32_t    data;
    uint32_t    checkptr;
    uint32_t    recvptr;
    uint32_t    sendptr;
    uint32_t    reserved0;
    uint32_t    reserved1;
    uint32_t    reserved2;
    uint16_t    capabilities;
    uint8_t     flags;
    uint8_t     device_type;
} __attribute__((packed)) CHAR_DEVICE;

typedef enum {
    STATE_DISCARD,
    STATE_AWAIT_SIG_1,
    STATE_AWAIT_LEN_HI,
    STATE_AWAIT_LEN_LO,
    STATE_FILL_DATA,
    STATE_CHECK_FILLED
} STATE;

typedef enum {
    KEYBOARD,
    GAMEPAD
} DEV_TYPE;


struct {

    bool        connected;
    uint16_t    vendor_id;
    uint16_t    product_id;
    DEV_TYPE    dev_type;

} USB_DEVICE[2];

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

extern void install_interrupt(CHAR_DEVICE *device);
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

int checkarray(uint8_t val, uint8_t* arr, uint8_t arrLen)
{

    for (int i = 0; i < arrLen; i++) {
        if (arr[i] == val) {
            return 1;
        }
    }

    return 0;

}

void process_strikes(uint8_t* new_keys) {

    CHAR_DEVICE *duart_a = mcGetDevice(0);

    for (int i = 2; i < 8; i++) {       // Outer loop - Last keys

        if (!checkarray(new_keys[i], last_ingest, 8)) fctprintf(mcSendDevice, duart_a, "%c", keys_upper[new_keys[i]]);

        // if (!strchr(new_keys,last_ingest[i])) { // OLD STRCHR CODE
           
        //     if (last_ingest[0] == 0x20) fctprintf(mcSendDevice, duart_a, "%c", keys_upper[new_keys[i]]);
        //     else fctprintf(mcSendDevice, duart_a, "%c", keys_lower[new_keys[i]]);

        // }

    }

    memcpy(last_ingest, new_keys, 8);

}

bool isSet(unsigned value, unsigned bitindex)
{
    return (value & (1 << bitindex)) != 0;
}

void process_gamepad(uint8_t* new_pad) {

    // printf("\nChecking Gamepad Data\n");
    // printf("Button byte: 0x%X\n", new_pad[3]);

    if (new_pad[2] != 0x80) {

        if (new_pad[2] == 0x00) printf("Up!\n");
        if (new_pad[2] == 0x04) printf("Down!\n");
        if (new_pad[2] == 0x06) printf("Left!\n");
        if (new_pad[2] == 0x02) printf("Right!\n");

    }

    if (new_pad[3] != 0x00) {

        if (isSet(new_pad[3], 0)) printf("A button!\n");
        if (isSet(new_pad[3], 1)) printf("B button!\n");
        if (isSet(new_pad[3], 2)) printf("X button!\n");
        if (isSet(new_pad[3], 3)) printf("Y button!\n");
        if (isSet(new_pad[3], 4)) printf("Left trigger!\n");
        if (isSet(new_pad[3], 5)) printf("Right trigger!\n");
        if (isSet(new_pad[3], 6)) printf("Left stick!\n");
        if (isSet(new_pad[3], 7)) printf("Right stick!\n");

    }

}

void process_final_packet(FinalPacket *p) {

        // if (p->messsage_type == USB_MSG_CONNECT) {
        
        //     USB_DEVICE[p->endpoint].connected = true;
        //     USB_DEVICE[p->endpoint].vendor_id = p.vendor_id;
        //     USB_DEVICE[p->endpoint].product_id = p.product_id;
        //     printf("Vendor ID 0x%02x, Product ID 0x%02x connected\n", USB_DEVICE[p->endpoint].vendor_id, USB_DEVICE[p->endpoint].product_id);
        
        // }
        
        if (p->message_type == USB_MSG_CONNECT) {        
            USB_DEVICE[p->endpoint].connected = true;
            USB_DEVICE[p->endpoint].vendor_id = p->id_vendor_lo | p->id_vendor_hi << 8;
            USB_DEVICE[p->endpoint].product_id = p->id_product_lo | p->id_product_hi << 8;
            printf("Vendor ID 0x%02x, Product ID 0x%02x connected\n", USB_DEVICE[p->endpoint].vendor_id, USB_DEVICE[p->endpoint].product_id);        
        }

        else if (p->messsage_type == USB_MSG_DISCONNECT) printf("Device disconnected\n");

        else if (p->messsage_type == USB_MSG_REPORT) {

            if ((p->packet_data[8] == 0x14) && (p->packet_data[9] == 0x72)) {
                process_gamepad(&(p->packet_data[0]));
            } else process_strikes(&(p->packet_data[0]));

        }

}

// void process_raw_packet(uint8_t *raw_packet) {
        
//     FinalPacket *p;
    
//     p->endpoint     =   raw_packet[5];
//     p->message_type =   raw_packet[2];
//     p->hid_type     =   raw_packet[3];
//     p->device_type  =   raw_packet[4];
//     p->data_length  =   raw_packet[0];
//     p->data_length |=   raw_packet[1] << 8;
//     p->vendor_id    =   raw_packet[6];   
//     p->vendor_id   |=   raw_packet[7] << 8;
//     p->product_id   =   raw_packet[8];
//     p->product_id  |=   raw_packet[9] << 8;
    
//     memcpy(&(p->packet_data), &(raw_packet[10]), sizeof(p->packet_data));

//     process_final_packet(p);

// }

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
            state->state = STATE_AWAIT_LEN_HI;
            state->packet_ptr = 0;
        } else {
            // invalid - back to discard
            DEBUGF("(STATE_AWAIT_SIG_1      -> STATE_DISCARD [0x%02x]\r\n\r\n", data);
            state->state = STATE_DISCARD;
        }

        break;

    case STATE_AWAIT_LEN_HI:
        DEBUGF("(STATE_AWAIT_LEN_HI     -> STATE_AWAIT_LEN_LO [0x%02x]\r\n\r\n", data);
        state->remain_len = data;
        state->state = STATE_AWAIT_LEN_LO;
        state->packet[state->packet_ptr++] = data;
        break;

    case STATE_AWAIT_LEN_LO:
        DEBUGF("(STATE_AWAIT_LEN_LO     -> STATE_FILL_DATA [0x%02x]\r\n\r\n", data);
        state->remain_len |= data << 8;
        state->remain_len += 8;
        state->state = STATE_FILL_DATA;
        state->packet[state->packet_ptr++] = data;
        break;

    case STATE_FILL_DATA:
        state->packet[state->packet_ptr++] = data;

        if (--state->remain_len == 0) {
            // All data transferred, expect a "\n" to close the communication.
            DEBUGF("(STATE_FILL_DATA         -> STATE_DISCARD (and process) [0x%02x] [remain_len = 0x%04x]\r\n\r\n", data, state->remain_len);
            state->state = STATE_CHECK_FILLED;
            
        } 
#ifdef DEBUG_PACKETS
        else {
            DEBUGF("                        #  FILL [0x%02x] [remain_len = 0x%04x]\r\n\r\n", data, state->remain_len);
        }
#endif

        break;

	case STATE_CHECK_FILLED:
	   //state->packet[state->packet_ptr++] = data;	
	   //if (state->packet[state->packet_ptr] == '\n') {
         if (data == '\n') {
               state->state = STATE_DISCARD;
               process_raw_packet(state->packet);
           } else {
            DEBUGF("BAD PACKET?");
            state->state = STATE_DISCARD;
           }
           break;
        
    }
}

void process_incoming(State *state) {
    static uint8_t buffer[1024];
#ifdef DEBUG_PACKETS
#ifdef DEBUG_VERBOSE
    CHAR_DEVICE *duart_a = mcGetDevice(0);
#endif
#endif

    uint16_t count = unbuffer(buffer);      //Pull the ring buffer
    if (count == 0) return;

#ifdef DEBUG_PACKETS
#ifdef DEBUG_VERBOSE
    for (int i = 0; i < count; i++) {
        fctprintf(mcSendDevice, duart_a, "0x%02x ", buffer[i]);
    }
    fctprintf(mcSendDevice, duart_a, "\r\n\r\n");
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
            CHAR_DEVICE *dev = mcGetDevice(i);

            printf("data    : 0x%08lx\n", dev->data);
            printf("checkptr: 0x%08lx\n", dev->checkptr);
            printf("recvptr : 0x%08lx\n", dev->recvptr);
            printf("sendptr : 0x%08lx\n", dev->sendptr);

            // printf("Printing to device %d\n", i);
            // fctprintf(mcSendDevice, dev, "Hello, World\n"); 
            printf("\n\n");
        }

        printf("Done\n");

        CHAR_DEVICE *duart_b = mcGetDevice(1);

        install_interrupt(duart_b);

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

        CHAR_DEVICE *duart_a = mcGetDevice(0);
        fctprintf(mcSendDevice, duart_a, "\f");

        State state;
        state.state = STATE_DISCARD;

        while (true) {            
            process_incoming(&state);
#ifdef DEBUG_HEARTBEAT        
            fctprintf(mcSendDevice, duart_a, "*");
#endif        
        }
    } else {
        printf("No character device support detected\n");
    }
}

