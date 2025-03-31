#include "rpi.h"

void notmain(void) {

    // aditi colors: 6: purple, 7: blue, 8: green, 9: yellow, 10: orange
    // input single is to the right of the 4-button complex

    const int input_single = 6;  
    const int input_right = 7;
    const int input_bottom = 8;
    const int input_top = 9;
    const int input_left = 10;

    // do in the middle in case some interference
    gpio_set_input(input_single);
    gpio_set_input(input_right);
    gpio_set_input(input_bottom);
    gpio_set_input(input_top);
    gpio_set_input(input_left);

    unsigned v = 0;
    for(int i = 0; i < 20; i++) {
        // read the input gpio
        int ret = gpio_read(input_single); // TODO test the other input buttons!; todo write debouncer
        printk("gpio_read = ");
        printk(ret==0?"0":"1");
        printk("\n");

        // hold so can see visually (humans are slow)
        uint32_t delay_dur = 2000;
        delay_ms(delay_dur);
    }
}
