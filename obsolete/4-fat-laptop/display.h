#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include <stdint.h>
#include "i2c.h"
#include "pi-sd.h"
#include "fat32.h"
#include "rpi.h" // automatically includes gpio, fat32 stuff

// Display dimensions
#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64

// SSD1306 commands
#define SSD1306_MEMORYMODE           0x20
#define SSD1306_COLUMNADDR           0x21
#define SSD1306_PAGEADDR             0x22
#define SSD1306_SETCONTRAST          0x81
#define SSD1306_CHARGEPUMP           0x8D
#define SSD1306_SEGREMAP             0xA0
#define SSD1306_DISPLAYALLON_RESUME  0xA4
#define SSD1306_DISPLAYALLON         0xA5
#define SSD1306_NORMALDISPLAY        0xA6
#define SSD1306_INVERTDISPLAY        0xA7
#define SSD1306_SETMULTIPLEX         0xA8
#define SSD1306_DISPLAYOFF           0xAE
#define SSD1306_DISPLAYON            0xAF
#define SSD1306_COMSCANINC           0xC0
#define SSD1306_COMSCANDEC           0xC8
#define SSD1306_SETDISPLAYOFFSET     0xD3
#define SSD1306_SETDISPLAYCLOCKDIV   0xD5
#define SSD1306_SETPRECHARGE         0xD9
#define SSD1306_SETCOMPINS           0xDA
#define SSD1306_SETVCOMDETECT        0xDB
#define SSD1306_SWITCHCAPVCC         0x02

// Color values
#define BLACK                0
#define WHITE                1
#define INVERSE              2

// I2C address for SSD1306
#define SSD1306_I2C_ADDR     0x3C  // Confirmed working address

// Function prototypes
void display_init(void);
void display_clear(void);
void display_update(void);
void display_set_pixel(int16_t x, int16_t y, uint8_t color);
void display_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color);
void display_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
void display_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
void display_draw_circle(int16_t x0, int16_t y0, int16_t r, uint8_t color);
void display_fill_circle(int16_t x0, int16_t y0, int16_t r, uint8_t color);
void display_draw_char(int16_t x, int16_t y, unsigned char c, uint8_t color, uint8_t bg, uint8_t size);
void display_write(int16_t x, int16_t y, const char *text, uint8_t color, uint8_t bg, uint8_t size);
void display_invert(uint8_t invert);
void display_command(uint8_t cmd);


// PBM (=image) stuff
void display_draw_pbm(const uint8_t *pbm_data, uint16_t size);
int is_pbm_file(const char *filename);

typedef struct {
    pi_dirent_t entry;         // The actual directory entry
    void* parent;              // Pointer to parent directory entry
} ext_dirent_t;

#endif // __DISPLAY_H__