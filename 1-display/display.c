#include "display.h"
#include "font.h"
#include "rpi.h"
#include <stdint.h>

// Buffer for the display (128x64 pixels = 1024 bytes)
static uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

/**
 * Helper function for 16-bit absolute value
 */
static int16_t abs16(int16_t x) {
    return (x < 0) ? -x : x;
}

/**
 * Send a command to the SSD1306 display
 */
void display_command(uint8_t cmd) {
    uint8_t buffer[2];
    buffer[0] = 0x00; // Command mode (Co = 0, D/C = 0)
    buffer[1] = cmd;
    i2c_write(SSD1306_I2C_ADDR, buffer, 2);
}

/**
 * Send data to the SSD1306 display
 */
static void display_data(uint8_t *data, uint16_t size) {
    // Create a static buffer on the stack instead of using kmalloc
    // This avoids heap allocation issues
    // Maximum size we would send is 1024 bytes (display buffer) + 1 command byte
    static uint8_t temp_buffer[1025]; // Static buffer large enough for our needs
    
    // Make sure we don't exceed our buffer size
    if (size + 1 > sizeof(temp_buffer)) {
        printk("ERROR: Data too large for buffer\n");
        return;
    }
    
    temp_buffer[0] = 0x40; // Data mode (Co = 0, D/C = 1)
    memcpy(temp_buffer + 1, data, size);
    
    i2c_write(SSD1306_I2C_ADDR, temp_buffer, size + 1);
}

/**
 * Initialize the SSD1306 display
 */
void display_init(void) {
    // Initialize I2C
    i2c_init();
    
    printk("Initializing SSD1306 OLED display...\n");
    
    // Wait for the display to settle (important for reliable startup)
    delay_ms(100);
    
    // Initialize display
    display_command(SSD1306_DISPLAYOFF);
    display_command(SSD1306_SETDISPLAYCLOCKDIV);
    display_command(0x80); // The suggested ratio 0x80
    display_command(SSD1306_SETMULTIPLEX);
    display_command(SSD1306_HEIGHT - 1);
    display_command(SSD1306_SETDISPLAYOFFSET);
    display_command(0x0);  // no offset
    display_command(0x40); // start line address = 0
    display_command(SSD1306_CHARGEPUMP);
    display_command(0x14); // using internal VCC
    display_command(SSD1306_MEMORYMODE);
    display_command(0x00); // horizontal addressing mode
    display_command(SSD1306_SEGREMAP | 0x1); // column address 127 is mapped to SEG0
    display_command(SSD1306_COMSCANDEC); // remapped mode - scan from COM[N-1] to COM0
    display_command(SSD1306_SETCOMPINS);
    display_command(0x12); // COM pins hardware configuration, alternative COM pin configuration
    display_command(SSD1306_SETCONTRAST);
    display_command(0xCF); // contrast value (0-255)
    display_command(SSD1306_SETPRECHARGE);
    display_command(0xF1); // pre-charge period (0x22 if external VCC)
    display_command(SSD1306_SETVCOMDETECT);
    display_command(0x40); // vcomh register level
    display_command(SSD1306_DISPLAYALLON_RESUME);
    display_command(SSD1306_NORMALDISPLAY);
    display_command(0x2E); // Deactivate scroll
    display_command(SSD1306_DISPLAYON);
    
    // Clear the display buffer
    display_clear();
    display_update();
    
    printk("SSD1306 OLED display initialized\n");
}

/**
 * Clear the display buffer
 */
void display_clear(void) {
    memset(buffer, 0, sizeof(buffer));
}

/**
 * Update the display with the buffer contents
 */
void display_update(void) {
    // Set column address range
    display_command(SSD1306_COLUMNADDR);
    display_command(0);                  // Column start address
    display_command(SSD1306_WIDTH - 1);  // Column end address
    
    // Set page address range
    display_command(SSD1306_PAGEADDR);
    display_command(0);                                  // Page start address
    display_command((SSD1306_HEIGHT / 8) - 1);          // Page end address
    
    // Send the buffer data
    display_data(buffer, sizeof(buffer));
}

/**
 * Set a pixel in the buffer
 */
void display_set_pixel(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT) {
        return;
    }
    
    // Map pixel coordinates to buffer byte position and bit position
    uint16_t byte_idx = x + (y / 8) * SSD1306_WIDTH;
    uint8_t bit_pos = y % 8;
    
    if (color == WHITE) {
        buffer[byte_idx] |= (1 << bit_pos);
    } else if (color == BLACK) {
        buffer[byte_idx] &= ~(1 << bit_pos);
    } else if (color == INVERSE) {
        buffer[byte_idx] ^= (1 << bit_pos);
    }
}

/**
 * Draw a line from (x0,y0) to (x1,y1)
 * Using Bresenham's line algorithm
 */
void display_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    int16_t steep = abs16(y1 - y0) > abs16(x1 - x0);
    
    if (steep) {
        // Swap x0, y0
        int16_t temp = x0;
        x0 = y0;
        y0 = temp;
        
        // Swap x1, y1
        temp = x1;
        x1 = y1;
        y1 = temp;
    }
    
    if (x0 > x1) {
        // Swap x0, x1
        int16_t temp = x0;
        x0 = x1;
        x1 = temp;
        
        // Swap y0, y1
        temp = y0;
        y0 = y1;
        y1 = temp;
    }
    
    int16_t dx = x1 - x0;
    int16_t dy = abs16(y1 - y0);
    int16_t err = dx / 2;
    int16_t ystep;
    
    if (y0 < y1) {
        ystep = 1;
    } else {
        ystep = -1;
    }
    
    for (; x0 <= x1; x0++) {
        if (steep) {
            display_set_pixel(y0, x0, color);
        } else {
            display_set_pixel(x0, y0, color);
        }
        
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

/**
 * Draw a rectangle
 */
void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    display_draw_line(x, y, x + w - 1, y, color);
    display_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    display_draw_line(x, y, x, y + h - 1, color);
    display_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

/**
 * Draw a filled rectangle
 */
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    for (int16_t i = x; i < x + w; i++) {
        for (int16_t j = y; j < y + h; j++) {
            display_set_pixel(i, j, color);
        }
    }
}

/**
 * Draw a circle
 */
void display_draw_circle(int16_t x0, int16_t y0, int16_t r, uint8_t color) {
    // Using the midpoint circle algorithm
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    display_set_pixel(x0, y0 + r, color);
    display_set_pixel(x0, y0 - r, color);
    display_set_pixel(x0 + r, y0, color);
    display_set_pixel(x0 - r, y0, color);
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        display_set_pixel(x0 + x, y0 + y, color);
        display_set_pixel(x0 - x, y0 + y, color);
        display_set_pixel(x0 + x, y0 - y, color);
        display_set_pixel(x0 - x, y0 - y, color);
        display_set_pixel(x0 + y, y0 + x, color);
        display_set_pixel(x0 - y, y0 + x, color);
        display_set_pixel(x0 + y, y0 - x, color);
        display_set_pixel(x0 - y, y0 - x, color);
    }
}

/**
 * Fill a circle
 */
void display_fill_circle(int16_t x0, int16_t y0, int16_t r, uint8_t color) {
    display_draw_line(x0, y0 - r, x0, y0 + r, color);
    
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        display_draw_line(x0 + x, y0 - y, x0 + x, y0 + y, color);
        display_draw_line(x0 - x, y0 - y, x0 - x, y0 + y, color);
        display_draw_line(x0 + y, y0 - x, x0 + y, y0 + x, color);
        display_draw_line(x0 - y, y0 - x, x0 - y, y0 + x, color);
    }
}

/**
 * Draw a character
 */
void display_draw_char(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size) {
    if (c < 32 || c > 126) c = '?';
    
    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = font[(c - 32) * 5 + i];
        
        for (int8_t j = 0; j < 8; j++) {
            if (line & 0x1) {
                if (size == 1) {
                    display_set_pixel(x + i, y + j, color);
                } else {
                    display_fill_rect(x + i * size, y + j * size, size, size, color);
                }
            } else if (bg != color) {
                if (size == 1) {
                    display_set_pixel(x + i, y + j, bg);
                } else {
                    display_fill_rect(x + i * size, y + j * size, size, size, bg);
                }
            }
            
            line >>= 1;
        }
    }
}

/**
 * Write text
 */
void display_write(int16_t x, int16_t y, const char *text, uint8_t color, uint8_t bg, uint8_t size) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;
    
    while (*text) {
        if (*text == '\n') {
            cursor_x = x;
            cursor_y += size * 8;
        } else if (*text == '\r') {
            // Skip carriage returns
        } else {
            display_draw_char(cursor_x, cursor_y, *text, color, bg, size);
            cursor_x += size * 6;
            
            if (cursor_x > (SSD1306_WIDTH - size * 6)) {
                cursor_x = x;
                cursor_y += size * 8;
            }
        }
        
        text++;
    }
}

/**
 * Invert the display
 */
void display_invert(uint8_t invert) {
    if (invert) {
        display_command(SSD1306_INVERTDISPLAY);
    } else {
        display_command(SSD1306_NORMALDISPLAY);
    }
}




void notmain(void) {
    printk("Starting OLED Display Test\n");
    
    // Initialize the display
    printk("Initializing display at I2C address 0x%x\n", SSD1306_I2C_ADDR);
    display_init();
    printk("Display initialized\n");
    
    // Clear the display
    display_clear();
    display_update();
    printk("Display cleared\n");
    
    // Draw some test patterns
    printk("Drawing test pattern\n");
    
    // Draw a border around the screen
    display_draw_rect(0, 0, SSD1306_WIDTH, SSD1306_HEIGHT, WHITE);
    
    // Draw text
    display_write(10, 5, "SSD1306 OLED", WHITE, BLACK, 1);
    display_write(10, 15, "Test Pattern", WHITE, BLACK, 1);
    
    // Draw some shapes
    display_draw_rect(10, 30, 30, 15, WHITE);
    display_fill_rect(50, 30, 30, 15, WHITE);
    display_draw_circle(100, 35, 10, WHITE);
    
    // Draw some lines
    display_draw_line(10, 50, 100, 55, WHITE);
    display_draw_line(10, 60, 100, 55, WHITE);
    
    // Update the display
    display_update();
    printk("Test pattern displayed\n");
    
    // Wait a bit then clear and show a second pattern
    delay_ms(2000);
    
    // Clear and show another pattern
    display_clear();
    
    // Draw large text centered
    display_write(30, 20, "IT WORKS!", WHITE, BLACK, 2);
    
    // Draw some dots at the corners
    display_set_pixel(0, 0, WHITE);
    display_set_pixel(SSD1306_WIDTH-1, 0, WHITE);
    display_set_pixel(0, SSD1306_HEIGHT-1, WHITE);
    display_set_pixel(SSD1306_WIDTH-1, SSD1306_HEIGHT-1, WHITE);
    
    // Update the display
    display_update();
    printk("Second pattern displayed\n");
    
    printk("OLED Display Test completed successfully\n");
}