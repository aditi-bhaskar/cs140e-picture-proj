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


// PBM stuff
/**
 * Check if a file is a PBM file based on its extension
 */
int is_pbm_file(const char *filename) {
    // Get the length of the filename
    int len = strlen(filename);
    
    // Check if it ends with .PBM or .pbm
    if (len >= 4) { // lowercase should never happen but can't hurt to check
        if ((filename[len-4] == '.' && 
             (filename[len-3] == 'P' || filename[len-3] == 'p') &&
             (filename[len-2] == 'B' || filename[len-2] == 'b') &&
             (filename[len-1] == 'M' || filename[len-1] == 'm'))) {
            return 1;
        }
    }
    return 0;
}


/**
 * Parse and display a PBM file
 * Supports both P1 (ASCII) and P4 (binary) formats
 */
void display_draw_pbm(const uint8_t *pbm_data, uint16_t size) {
    // Clear the display first
    display_clear();
    
    // Check if we have valid data
    if (!pbm_data || size < 10) {
        display_write(10, 20, "Invalid PBM file", WHITE, BLACK, 1);
        display_update();
        return;
    }
    
    // Check for PBM magic number
    if (pbm_data[0] != 'P' || (pbm_data[1] != '1' && pbm_data[1] != '4')) {
        display_write(10, 20, "Not a PBM file", WHITE, BLACK, 1);
        display_update();
        return;
    }
    
    // PBM format type
    int binary_format = (pbm_data[1] == '4');
    
    // Parse header to get width and height
    int width = 0, height = 0;
    int pos = 2;
    int parsing_width = 1;
    
    // Skip any whitespace
    while (pos < size && (pbm_data[pos] == ' ' || pbm_data[pos] == '\t' || 
                          pbm_data[pos] == '\n' || pbm_data[pos] == '\r')) {
        pos++;
    }
    
    // Parse width
    while (pos < size && pbm_data[pos] >= '0' && pbm_data[pos] <= '9') {
        width = width * 10 + (pbm_data[pos] - '0');
        pos++;
    }
    
    // Skip any whitespace
    while (pos < size && (pbm_data[pos] == ' ' || pbm_data[pos] == '\t' || 
                          pbm_data[pos] == '\n' || pbm_data[pos] == '\r')) {
        pos++;
    }
    
    // Parse height
    while (pos < size && pbm_data[pos] >= '0' && pbm_data[pos] <= '9') {
        height = height * 10 + (pbm_data[pos] - '0');
        pos++;
    }
    
    // Skip any whitespace to get to the start of data
    while (pos < size && (pbm_data[pos] == ' ' || pbm_data[pos] == '\t' || 
                          pbm_data[pos] == '\n' || pbm_data[pos] == '\r')) {
        pos++;
    }
    
    // Check if we have valid dimensions
    if (width <= 0 || height <= 0 || width > SSD1306_WIDTH || height > SSD1306_HEIGHT) {
        display_write(10, 20, "Invalid PBM size", WHITE, BLACK, 1);
        display_write(10, 30, "Max: 128x64", WHITE, BLACK, 1);
        display_update();
        return;
    }
    
    // Center
    int x_offset = (SSD1306_WIDTH - width) / 2;
    int y_offset = (SSD1306_HEIGHT - height) / 2;
    
    if (binary_format) {
        int row_bytes = (width + 7) / 8; 
        
        for (int y = 0; y < height; y++) {
            if (pos + row_bytes > size) break; // Ensure we don't read past the end
            
            for (int x = 0; x < width; x++) {
                int byte_pos = x / 8;
                int bit_pos = 7 - (x % 8); // MSB first in PBM
                
                if (pos + byte_pos < size) {
                    uint8_t pixel = (pbm_data[pos + byte_pos] >> bit_pos) & 0x01;
                    // In PBM, 1 is black and 0 is white, but our display uses opposite convention
                    display_set_pixel(x + x_offset, y + y_offset, pixel ? WHITE : BLACK);
                }
            }
            pos += row_bytes;
        }
    } else {
        // ASCII
        int x = 0, y = 0;
        
        while (pos < size && y < height) {
            // Skip whitespace
            while (pos < size && (pbm_data[pos] == ' ' || pbm_data[pos] == '\t' || 
                                  pbm_data[pos] == '\n' || pbm_data[pos] == '\r')) {
                pos++;
            }
            
            if (pos < size) {
                if (pbm_data[pos] == '0' || pbm_data[pos] == '1') {
                    // In PBM, 1 is black and 0 is white, but our display uses opposite convention
                    uint8_t pixel = (pbm_data[pos] == '1') ? WHITE : BLACK;
                    display_set_pixel(x + x_offset, y + y_offset, pixel);
                    
                    x++;
                    if (x >= width) {
                        x = 0;
                        y++;
                    }
                }
                pos++;
            }
        }
    }
    
    // Update the display with the rendered image
    display_update();
}

// ----------- DRAWING STUFF -----------
// Drawing mode constants
#define DRAWING_MODE_OFF  0
#define DRAWING_MODE_ON   1

// Drawing cursor position and state
static int cursor_x = 0;     // Start at top-left of the image area
static int cursor_y = 12;    // Just below the header line
static int drawing_mode = DRAWING_MODE_OFF;
static int cursor_speed = 1;  // Pixels to move per button press

/**
 * Draw the cursor at the current position
 * In drawing mode, cursor is a single pixel dot for precise placement
 */
void display_draw_cursor(int x, int y, int drawing) {
    if (drawing) {
        // In drawing mode, use just a single pixel
        if (x >= 0 && x < SSD1306_WIDTH && y >= 0 && y < SSD1306_HEIGHT) {
            display_set_pixel(x, y, INVERSE);
        }
    } else {
        // In navigation mode, use a small dot (3x3 pixels)
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (x + i >= 0 && x + i < SSD1306_WIDTH && 
                    y + j >= 0 && y + j < SSD1306_HEIGHT) {
                    display_set_pixel(x + i, y + j, INVERSE);
                }
            }
        }
    }
}

