#ifndef BITBANG_I2C_H
#define BITBANG_I2C_H

#include "rpi.h"  // for gpio
// helpful reading: https://www.ti.com/lit/an/slva704/slva704.pdf

// use these pins bc this is how we connect them
#define SDA_PIN 2
#define SCL_PIN 3

////////////////////////////////////////////////
// init i2c
////////////////////////////////////////////////

// this init operation can be repeated (idempotent)
void i2c_init(void) {
    gpio_set_output(SDA_PIN);
    gpio_set_output(SCL_PIN);
    gpio_set_on(SDA_PIN);
    gpio_set_on(SCL_PIN);
    
    // short delay to prevent issues
    delay_ms(10);
}

////////////////////////////////////////////////
// writing/reading single bits and bytes
////////////////////////////////////////////////

void i2c_start(void) {
    // start sequence: make sda go low while scl is high
    gpio_write(SDA_PIN, 1);
    gpio_write(SCL_PIN, 1);
    gpio_write(SDA_PIN, 0);

    // then turn scl low 
    // because it needs to be low while we write bits
    gpio_write(SCL_PIN, 0);
}

void i2c_stop(void) {
    // make sda go high when scl is high
    gpio_write(SDA_PIN, 0);
    gpio_write(SCL_PIN, 1);
    gpio_write(SDA_PIN, 1);
}

int i2c_write_byte(uint8_t byte) {

    // write every single bit per each clock cycle on the scl pin
    for (int i = 0; i < 8; i++) {
        gpio_write(SDA_PIN, (byte & 0x80) != 0); // isolates the msb of the byte
        gpio_write(SCL_PIN, 1);
        gpio_write(SCL_PIN, 0);
        byte = byte << 1; // shift byte over so the next bit becomes accessible
    }

    // read ack from device (in our case, the display)
    gpio_set_input(SDA_PIN);
    gpio_write(SCL_PIN, 1);
    int ack = !gpio_read(SDA_PIN);
    gpio_write(SCL_PIN, 0);
    gpio_set_output(SDA_PIN);

    return ack;
}

uint8_t i2c_read_byte(int send_ack) {
    // read from the sda pin
    gpio_set_input(SDA_PIN);

    uint8_t byte = 0;
    // shifting over the incoming bits to store it as a byte
    for (int i = 0; i < 8; i++) {
        gpio_write(SCL_PIN, 1);
        byte = (byte << 1) | gpio_read(SDA_PIN);
        gpio_write(SCL_PIN, 0);
    }

    // set sda back to output, since that's its default state
    gpio_set_output(SDA_PIN);
    gpio_write(SDA_PIN, !send_ack); // ack if it will read more
    gpio_write(SCL_PIN, 1); // toggle scl up and down as end of sequence
    gpio_write(SCL_PIN, 0);

    return byte;
}


////////////////////////////////////////////////////////////////////////
// read and write functions most commonly used by our display lib:
////////////////////////////////////////////////////////////////////////

int i2c_write(unsigned addr, uint8_t data[], unsigned nbytes) {
    // write a start bit
    i2c_start();

    // stop if no ack
    if (!i2c_write_byte(addr << 1)) {
        printk("ERROR! NO DEVICE ACK ON WRITE!");
        i2c_stop();
        return 0; 
    }

    // write as many bytes as possible
    for (unsigned i = 0; i < nbytes; i++) {
        if (!i2c_write_byte(data[i])) {
            i2c_stop();
            return 0;
        }
    }

    // write stop bit
    i2c_stop();
    return 1;
}

int i2c_read(unsigned addr, uint8_t data[], unsigned nbytes) {
    // write start bit
    i2c_start();
    
    // stop if the device doesn't ack a read request
    if (!i2c_write_byte((addr << 1) | 1)) { 
        printk("ERROR! NO DEVICE ACK ON READ!");
        i2c_stop();
        return 0;
    }

    // read the bytes!
    for (unsigned i = 0; i < nbytes; i++)
        data[i] = i2c_read_byte(i < nbytes - 1);

    // write stop bit
    i2c_stop();
    return 1;
}


#endif
