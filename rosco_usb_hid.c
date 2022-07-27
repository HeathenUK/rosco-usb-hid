#include "ROSCO_CH559.h"

int main(int argc, char **argv) {

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
        

        printf("String: %s", ugets(&state, 50));
        // if (check_pad(&state)) {
        //     BUTTONS latest = read_pad(&state);
        //     printf("%u ", latest.dpad);
        //     printf("%u ", latest.apad);
        //     printf("%u\n", latest.xpad);
        // }

        // if (check_key(&state)) {

        //     printf("%c", read_key(&state));

        // }

    }
}