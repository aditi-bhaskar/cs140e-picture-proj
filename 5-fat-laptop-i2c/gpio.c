/*
 * Implement the following routines to set GPIO pins to input or 
 * output, and to read (input) and write (output) them.
 *  - DO NOT USE loads and stores directly: only use GET32 and 
 *    PUT32 to read and write memory.  
 *  - DO USE the minimum number of such calls.
 * (Both of these matter for the next lab.)
 *
 * See rpi.h in this directory for the definitions.
 */
#include "rpi.h"

// see broadcomm documents for magic addresses.
//
// if you pass addresses as:
//  - pointers use put32/get32.
//  - integers: use PUT32/GET32.
//  semantics are the same.
enum {
    GPIO_BASE = 0x20200000,
    gpio_set0  = (GPIO_BASE + 0x1C),
    gpio_clr0  = (GPIO_BASE + 0x28),
    gpio_lev0  = (GPIO_BASE + 0x34),
    gpio_fsel0 = (GPIO_BASE) 
    // <you may need other values.>
};

//
// Part 1 implement gpio_set_on, gpio_set_off, gpio_set_output
//

// set <pin> to be an output pin.
//
// note: fsel0, fsel1, fsel2 are contiguous in memory, so you
// can (and should) use array calculations!
void gpio_set_output(unsigned pin) {
    if(pin >= 32 && pin != 47)
        return;

    uint32_t pin_set = pin / 10;
    pin = pin % 10;

    uint32_t addr = gpio_fsel0 + pin_set * 0x4;

    uint32_t val = GET32(addr);

    uint32_t mask = 0b111 << 3*pin; // clear the bits
    mask = ~mask;
    val &= mask;

    uint32_t set = 0b1 << 3*pin; // set the correct bits
    val |= set;
 
    PUT32(addr, val);

}

// set GPIO <pin> on.
void gpio_set_on(unsigned pin) {
    if(pin >= 32 && pin != 47)
        return;

    uint32_t val = 0b1 << pin%32;
    uint32_t addr = gpio_set0 + pin/32 * 4;
    PUT32(addr, val);
}

// set GPIO <pin> off
void gpio_set_off(unsigned pin) {
    if(pin >= 32 && pin != 47)
        return;

    uint32_t val = 0b1 << pin%32;
    uint32_t addr = gpio_clr0 + pin/32 * 4; // 4 bc 4 bytes
   
    PUT32(addr, val);
}

// set <pin> to <v> (v \in {0,1})
void gpio_write(unsigned pin, unsigned v) {
    if(v)
        gpio_set_on(pin);
    else
        gpio_set_off(pin);
}

//
// Part 2: implement gpio_set_input and gpio_read
//

// set <pin> to input.
void gpio_set_input(unsigned pin) {
  if(pin >= 32 && pin != 47)
    return;

  uint32_t pin_set = pin / 10;
  pin = pin % 10;

  uint32_t addr = gpio_fsel0 + pin_set * 0x4;

  uint32_t val = GET32(addr);

  uint32_t mask = 0b111 << 3*pin; // clear the bits
  mask = ~mask;
  val &= mask;

  // don't need to set the bit because it' alr 0b000

  PUT32(addr, val); 
 
}

// return the value of <pin>
int gpio_read(unsigned pin) {

  uint32_t pin_set = pin / 32;

  uint32_t addr = gpio_lev0 + pin_set * 0x4;
 
  uint32_t val = GET32(addr);
  val = val >> pin%32;
  val &= 0b1;

  return val;
}

//// extension


// get the pin mode 
// uint32_t gpio_get_mode(unsigned pin) {
//     if(pin >= 32 && pin != 47)
//         return;

//     uint32_t pin_set = pin / 10;
//     pin = pin % 10;

//     uint32_t addr = gpio_fsel0 + pin_set * 0x4;

//     uint32_t val = GET32(addr);
//     val = val >> 3*pin;
//     val &= 0b111;

//     return val;
// }