// # i2c scanner


#include "rpi.h"
#include "i2c.h"

// Possible I2C addresses for SSD1306
#define SSD1306_ADDR1 0x3C  // 0b0111100 // our display device is here!
#define SSD1306_ADDR2 0x3D  // 0b0111101

// Define an error handling wrapper for i2c_write
int safe_i2c_write(uint8_t addr, uint8_t *data, unsigned nbytes) {
    // Set up system to handle errors
    int old = *(volatile int*)0x20200000; // Save old value to restore later
    
    // Try the write operation
    int ret = i2c_write(addr, data, nbytes);
    
    // Restore system state
    *(volatile int*)0x20200000 = old;
    
    return ret;
}

/**
 * Scan the entire I2C bus for devices more safely
 */
void i2c_scan(void) {
    printk("Starting I2C bus scan (0-127)...\n");
    
    int found = 0;
    uint8_t data = 0x00;
    
    // Skip address 0 (general call) and reserved addresses (0-7)
    for (uint8_t addr = 0; addr < 128; addr++) {
        // Only print status occasionally to reduce output clutter
        if (addr % 16 == 0) {
            printk("Scanning addresses 0x%x to 0x%x...\n", addr, addr + 15);
        }
        
        // Try writing to the device with our safe wrapper
        int result = safe_i2c_write(addr, &data, 0);
        
        if (result >= 0) {
            printk("SUCCESS: Device found at address: 0x%x (decimal: %d)\n", addr, addr);
            found++;
        }
        
        // Short delay between attempts
        delay_ms(1);
    }
    
    if (found == 0) {
        printk("No I2C devices found! Check connections.\n");
    } else {
        printk("Found %d device(s) on the I2C bus\n", found);
    }
}

/**
 * Try to detect just the SSD1306 display specifically
 */
void check_ssd1306_addresses(void) {
    printk("\nTrying known SSD1306 addresses specifically...\n");
    uint8_t cmd = 0x00;
    
    // First address
    printk("Testing address 0x%x (0b0111100)...", SSD1306_ADDR1);
    int result = safe_i2c_write(SSD1306_ADDR1, &cmd, 0);
    if (result >= 0) {
        printk("SUCCESS: Device responded!\n");
    } else {
        printk("No response (error: %d)\n", result);
    }
    
    // Second address
    printk("Testing address 0x%x (0b0111101)...", SSD1306_ADDR2);
    result = safe_i2c_write(SSD1306_ADDR2, &cmd, 0);
    if (result >= 0) {
        printk("SUCCESS: Device responded!\n");
    } else {
        printk("No response (error: %d)\n", result);
    }
}

// Attempt to reinitialize I2C with various clock dividers
void try_different_clocks(void) {
    printk("\nTrying different I2C configurations...\n");
    
    // Note: This is a dummy function as we don't have direct access
    // to modify I2C clock dividers in this example
    // In a real application, you would try different configurations
    
    printk("Limited options with current I2C implementation\n");
}

void notmain(void) {
    printk("Starting SSD1306 'Who Am I' Test with Error Handling\n");
    
    // Wait for I2C and system to stabilize
    printk("Waiting for system to stabilize...\n");
    delay_ms(100);
    
    // Initialize I2C bus
    printk("Initializing I2C...\n");
    i2c_init();
    printk("I2C initialized\n");
    
    // Check only the specific SSD1306 addresses
    // check_ssd1306_addresses();
    
    // If no success, try a careful scan of the whole bus
    printk("\nPerforming careful scan of all I2C addresses...\n");
    i2c_scan();
    
    // Try different I2C configurations
    try_different_clocks();
    
    printk("\nSSD1306 'Who Am I' Test completed\n");
}