#include "rpi.h" // automatically includes gpio, fat32 stuff
#include "display.c"
#include "pi-sd.h"
#include "fat32.h"

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

void test_buttons(void){
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

static uint32_t min(uint32_t a, uint32_t b) {
    if (a < b) {
      return a;
    }
    return b;
}


void show_files(pi_directory_t files) {
    uint32_t num_entries_to_show = 4; // show 4 files at a time
    unsigned num_entries_in_dir = files.ndirents;
    uint32_t top_index = 0;
    uint32_t bot_index = 0;
    char text_to_display[18 * num_entries_to_show + 1]; // file name can be max 16 bytes long + 2 for enter, 1 at the end for null terminator
        

    while(1) {
        if (!gpio_read(input_left)) {
            // this is our sign to return back to home screen
            printk("broke out of show_files");
            break;
        }

        if (!gpio_read(input_bottom)) {
            if (top_index < num_entries_in_dir - num_entries_to_show)
                top_index++;
            delay_ms(500);
        }
        if (!gpio_read(input_top)) {
            if (top_index > 0)
                top_index--;
            delay_ms(500);
        }
        // input_right currently doesn't do anything
        bot_index = min(top_index + num_entries_to_show, num_entries_in_dir - 1);

        
        // Build the display text properly
        text_to_display[0] = '\0';
        int text_pos = 0;
        for (int i = top_index; i <= bot_index; i++) {
            pi_dirent_t *dirent = &files.dirents[i];
            
            // Copy the filename to buffer
            int j = 0;
            while (dirent->name[j] != '\0' && text_pos < sizeof(text_to_display) - 2) {
                text_to_display[text_pos++] = dirent->name[j++];
            }
            
            // Add newline if there's space
            if (text_pos < sizeof(text_to_display) - 2) {
                text_to_display[text_pos++] = '\n';
            }
            
            // Ensure null termination
            text_to_display[text_pos] = '\0';
        }
        
        display_clear();
        display_write(10, 20, text_to_display, WHITE, BLACK, 1);
        display_update();

        // Add a small delay to debounce buttons
        delay_ms(100);
    }
    // dirent->name has max 16 bytes
    // for (int i = 0; i < files.ndirents; i++) {
    //     pi_dirent_t *dirent = &files.dirents[i];
    //     if (dirent->is_dir_p) {
    //       printk("\tD: %s (cluster %d)\n", dirent->name, dirent->cluster_id);
    //     } else {
    //       printk("\tF: %s (cluster %d; %d bytes)\n", dirent->name, dirent->cluster_id, dirent->nbytes);
    //     }
    //   }
}

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

    printk("buttons initialized\n");
    //test_buttons();

    printk("Start of FAT shenanigans\n");

    kmalloc_init(FAT32_HEAP_MB);
    pi_sd_init();
  
    printk("Reading the MBR.\n");
    mbr_t *mbr = mbr_read();
  
    printk("Loading the first partition.\n");
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    assert(mbr_part_is_fat32(partition.part_type));
  
    printk("Loading the FAT.\n");
    fat32_fs_t fs = fat32_mk(&partition);
  
    printk("Loading the root directory.\n");
    pi_dirent_t root = fat32_get_root(&fs);
  
    printk("Listing files:\n");
    pi_directory_t files = fat32_readdir(&fs, &root);
    printk("Got %d files.\n", files.ndirents);

    // let's try and store the file names and show them on display
    show_files(files);
    display_clear();
    display_write(10, 20, "Returned from show_files", WHITE, BLACK, 1);
    display_update();



}
