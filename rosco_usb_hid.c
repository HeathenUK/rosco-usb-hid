#include "ROSCO_CH559.h"

static RingBuffer uart_a;
static RingBuffer uart_b;

int main(int argc, char **argv) {

    (void)argc;
    (void)argv;

    printf("\033*");
    printf("\f");

    // set up ring buffers
    uart_a.mask = 0x3ff;
    uart_b.mask = 0x3ff;

    CharDevice duart_b;
    mcGetDevice(1, &duart_b);
    install_interrupt(&duart_b, &uart_a, &uart_b);

    CharDevice duart_a;
    if (mcGetDevice(0, &duart_a)) fctprintf(mcSendDevice, &duart_a, "\f");

    State stateA;
    stateA.state = STATE_DISCARD;
    stateA.ringBuffer = &uart_a;

    State stateB;
    stateB.state = STATE_DISCARD;
    stateB.ringBuffer = &uart_b;

    init_usb();

    while (true) {            
        
        //Demo 1: fgets() equivalent. Will wait for either a \n terminated
        //string or a max length and then return that string.
        // char buffer[40];
        // printf("Prompt:> ");
        // ugets(&state, buffer, sizeof(buffer)-1);
        // printf("\nString: %s\n\n", buffer);
        
        //printf("\nString: %d\n\n", read_raw(&state));
            
        
        //Demo 2: get full state of last keyboard to send input. In this example, combine the state of the control key byte with the last key pressed to respond to Ctrl+C
        // struct KEYB inspect = get_kb(&state);
        // if (inspect.raw[0] == KEY_C && isSet(inspect.control_keys, KEY_MOD_RCTRL)) printf("Combo!");

        //Demo 3: checkchar() equivalents for pads and keyboards. Will return true if input is
        //is pending, false if not.

        if (check_pad(&stateA)) {
            BUTTONS latest = read_pad(&stateA);
            printf("%u ", latest.dpad);
            printf("%u ", latest.apad);
            printf("%u\n", latest.xpad);
        }

        if (check_pad(&stateB)) {
            BUTTONS latest = read_pad(&stateB);
            printf("%u ", latest.dpad);
            printf("%u ", latest.apad);
            printf("%u\n", latest.xpad);
        }

        // if (check_key(&state)) printf("%c", read_key(&state));

        //Demo 4: readchar() equivalent. Will block until input is pending
        //and then return it.
        
        //  printf("%c", read_key(&state));

    }
}
