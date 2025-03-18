// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#define WIDTH 128
#define HEIGHT 64

void set_pixel(unsigned char *bitmap, int x, int y, int value) {
    int byte_index = (y * WIDTH + x) / 8;
    int bit_index = 7 - (x % 8);
    
    if (value)  
        bitmap[byte_index] |= (1 << bit_index);  // Set bit to 1
    else
        bitmap[byte_index] &= ~(1 << bit_index); // Clear bit to 0
}

// TODO: use our filewrites to do this
void save_pbm(const char *filename, unsigned char *bitmap) {
    // FILE *file = fopen(filename, "wb");
    // if (!file) {
    //     perror("Failed to open file");
    //     return;
    // }

    // fprintf(file, "P4\n%d %d\n", WIDTH, HEIGHT);  // PBM Header
    // fwrite(bitmap, 1, (WIDTH * HEIGHT) / 8, file); // Write raw pixel data

    // fclose(file);
}

int main() {
    unsigned char bitmap[(WIDTH * HEIGHT) / 8];  
    memset(bitmap, 0xFF, sizeof(bitmap)); // Start with a white background (all 1s)

    // Example: Set some pixels
    set_pixel(bitmap, 10, 10, 0); // Set (10,10) to black
    set_pixel(bitmap, 20, 20, 0); // Set (20,20) to black
    set_pixel(bitmap, 30, 30, 0); // Set (30,30) to black

    // Save the file
    save_pbm("output.pbm", bitmap);

    printf("PBM file 'output.pbm' created!\n");
    return 0;
}