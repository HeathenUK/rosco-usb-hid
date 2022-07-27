#include "ROSCO_CH559.h"

int main(int argc, char **argv) {

    (void)argc;
    (void)argv;

    printf("\033*");
    printf("\f");

    CharDevice duart_b;
    mcGetDevice(1, &duart_b);
    install_interrupt(&duart_b);

    CharDevice duart_a;
    if (mcGetDevice(0, &duart_a)) fctprintf(mcSendDevice, &duart_a, "\f");

    State state;
    state.state = STATE_DISCARD;

    init_usb();

    while (true) {            
        
        //Demo 1: fgets() equivalent. Will wait for either a \n terminated
        //string or a max length and then return that string.
        char buffer[40];
        ugets(&state, buffer, sizeof(buffer)-1);
        printf("\nString: %s", buffer);
        
        //Demo 2: checkchar() equivalents. Will return true if input is
        //is pending, false if not.

        // if (check_pad(&state)) {
        //     BUTTONS latest = read_pad(&state);
        //     printf("%u ", latest.dpad);
        //     printf("%u ", latest.apad);
        //     printf("%u\n", latest.xpad);
        // }

        // if (check_key(&state)) printf("%c", read_key(&state));

        //Demo 3: readchar() equivalent. Will block until input is pending
        //and then return it.
        
        //  printf("%c", read_key(&state));


    }
}