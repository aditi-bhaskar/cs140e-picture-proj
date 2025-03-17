// DATASHEET: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf 
// SSD1306 = display name

#include "rpi.h"
#include "i2c.h"
#include "gpio.h"
#include "display.h"

// DISPLAY CONSTS AND VARS
// based on adafruit ssd1306 driver
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3D ///< See datasheet P19 for Address; 0x3D for 128x64, WHOAMI
#define CLK_DURING //  TODO: find page number for this
#define CLK_AFTER //  TODO: find page number for this


#define MB(x) ((x)*1024*1024)

#define SEG_HEAP MB(1)

uint8_t *buffer;


// WIRE CONSTS AND VARS
// https://github.com/codebendercc/arduino-library-files/blob/master/libraries/Wire
#define BUFFER_LENGTH 32

uint8_t rxBuffer[BUFFER_LENGTH];
uint8_t rxBufferIndex = 0;
uint8_t rxBufferLength = 0;

uint8_t txAddress = 0;
uint8_t txBuffer[BUFFER_LENGTH];
uint8_t txBufferIndex = 0;
uint8_t txBufferLength = 0;

uint8_t transmitting = 0;


#define WIRE_MAX 32 ///< Use common Arduino core default TODO maybe use a different number??


// #define BEGIN_TRANSMISSION transmitting = 1; txAddress = SCREEN_ADDRESS; txBufferIndex = 0; txBufferLength = 0;

// #define END_TRANSMISSION /*TODO!!!*/

void display_init() {
    // stuff for SDA, SCL?
    printk("Initializing display\n");
    return;
}

void display_commandList(const uint8_t *c, uint8_t n) {
    // TODO: WRITE THE WIRE_WRITE FUNCTION.. or use i2c write 
    // int i2c_write(unsigned addr, uint8_t data[], unsigned nbytes);

    uint8_t zeroes[] = {0x00};
    i2c_write(SCREEN_ADDRESS, zeroes, 1);
    uint16_t bytesOut = 1;

    while (n--) {
        if (bytesOut >= WIRE_MAX) {
            i2c_write(SCREEN_ADDRESS, zeroes, 1);
            // WIRE_WRITE((uint8_t)0x00); // Co = 0, D/C = 0
            bytesOut = 1;
        }
        WIRE_WRITE((*(const unsigned char *))(c++));
        // i2c_write(SCREEN_ADDRESS, zeroes, 1); // TODO: NEED TO READ SEQUENTIALLY NEXT BYTE of c instead of zeroes!!
        bytesOut++;
    }

}

void display_command1(uint8_t c) {
    // TODO: WRITE THE WIRE_WRITE FUNCTION.. or use i2c write 
    // int i2c_write(unsigned addr, uint8_t data[], unsigned nbytes);

    uint8_t zeroes[] = {0x00};
    i2c_write(SCREEN_ADDRESS, zeroes, 1);
    i2c_write(SCREEN_ADDRESS, zeroes, 1); // TODO pass c here instead

}

// return, reset, and periph begin were bools
// assume i2c is already initialized
// return 0 if no error, number otherwise
uint8_t display_begin(void) {
    uint8_t vcs = SSD1306_SWITCHCAPVCC;
    uint8_t addr = SCREEN_ADDRESS;

    // TODO: WHY +7 AND /8 ??
    
    kmalloc_init_set_start((void*) SEG_HEAP, MB(1));
    if ((!buffer) && !(buffer = (uint8_t *)kmalloc(SCREEN_WIDTH * ((SCREEN_HEIGHT + 7) / 8)))) return 1;

    uint8_t vcc_state = vcs;

    // TODO : write test cases to test any of this

    // TODO do we need reset?? YES WE NEED RESET

    // TRANSACTION_START // TODO: maybe, reset the i2c clk

    // Init sequence: TODO: understand init sequence (why done in this order?? wat does this mean??)
    static const uint8_t init1[] = {SSD1306_DISPLAYOFF,         // 0xAE
                                    SSD1306_SETDISPLAYCLOCKDIV, // 0xD5
                                    0x80, // the suggested ratio 0x80
                                    SSD1306_SETMULTIPLEX // 0xA8
                                    }; 
    display_commandList(init1, sizeof(init1));
    display_command1(SCREEN_HEIGHT - 1);

    
// static const uint8_t PROGMEM init2[] = {SSD1306_SETDISPLAYOFFSET, // 0xD3
//     0x0,                      // no offset
//     SSD1306_SETSTARTLINE | 0x0, // line #0
//     SSD1306_CHARGEPUMP};        // 0x8D
// ssd1306_commandList(init2, sizeof(init2));

// ssd1306_command1((vccstate == SSD1306_EXTERNALVCC) ? 0x10 : 0x14);

// static const uint8_t PROGMEM init3[] = {SSD1306_MEMORYMODE, // 0x20
//     0x00, // 0x0 act like ks0108
//     SSD1306_SEGREMAP | 0x1,
//     SSD1306_COMSCANDEC};
// ssd1306_commandList(init3, sizeof(init3));


    // clearDisplay();

    return 0;
}


void notmain(void) { 
    printk("Starting notmain\n");
    i2c_init_once();
    display_init();
    display_begin();
    return;

}
