#include "rpi.h" // automatically includes gpio, fat32 stuff
#include "display.c"

// aditi wire colors: 6: purple, 7: blue, 8: green, 9: yellow, 10: orange
// suze wire colors: TODO

#define BUTTON_SINGLE   's'
#define BUTTON_TOP      't'
#define BUTTON_RIGHT    'r'
#define BUTTON_LEFT     'l'
#define BUTTON_BOTTOM   'b'
#define BUTTON_NONE     'n'

#define input_single 6  
#define input_right 7
#define input_bottom 8
#define input_top 9
#define input_left 10

// TODO: measure state transition instead of state: checks whether it went not-pressed to pressed
int button_states[5][2] = {{0,0}, {0,0}, {0,0}, {0,0}, {0,0}}; // 0 means not pressed

void notmain(void) {

    printk("Starting Button + Display Integration Test\n");

    // display init 
    printk("Initializing display at I2C address 0x%x\n", SSD1306_I2C_ADDR);
    display_init();
    printk("Display initialized\n");
    
    // Clear the display
    display_clear();
    display_update();
    printk("Display cleared\n");

    // button init
    printk("Initializing buttons\n");

    // do in the middle in case some interference
    gpio_set_input(input_single);
    gpio_set_input(input_right);
    gpio_set_input(input_bottom);
    gpio_set_input(input_top);
    gpio_set_input(input_left);

    unsigned v = 0;
    while (1) {

        // read gpio
        char which_button = BUTTON_NONE;
        // read the input gpios; ! to measure whether it was pressed down
        if (!gpio_read(input_top)) which_button = BUTTON_TOP;
        if (!gpio_read(input_bottom)) which_button = BUTTON_BOTTOM;
        if (!gpio_read(input_right)) which_button = BUTTON_RIGHT;
        if (!gpio_read(input_left)) which_button = BUTTON_LEFT;
        if (!gpio_read(input_single)) which_button = BUTTON_SINGLE;

        char str[] = {which_button, '\0'};
        display_clear();
        display_write(10, 20, "button pressed:", WHITE, BLACK, 1);
        display_write(10, 45, str, WHITE, BLACK, 2);
        display_update();

    }
}
