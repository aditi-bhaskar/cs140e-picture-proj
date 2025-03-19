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


char unique_file_id = 65; // starts at: "A" // used when creating files


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

void create_file(fat32_fs_t *fs, pi_dirent_t *directory, pi_directory_t files) {

    // create a file; name it a random number like demo_{num}
    char filename[7] = {'d','e','m','o','_',unique_file_id,'\0'};
    unique_file_id++;

    pi_dirent_t *created_file = fat32_create(fs, directory, filename, 0); // 0=not a directory

    while(1) {
        if (!gpio_read(input_single)) {
            // save file & return
            printk("broke out of create_file");
            break;
        }

        pi_file_t *file = fat32_read(fs, directory, filename);

        // these buttons write to file
        if (!gpio_read(input_left)) {
            // write : "prefetch flush* "
            char *words = "*prefetch flush* ";
            (file->data[file->n_data]) = *words;
            // file->n_data += words.size(); // TODO DO SOMETHING LIKE THIS
            int writ = fat32_write(fs, files.dirents, filename, file);
            printk("wrote prefetch flush to file \n");
            // TODO display this stuff on screen
            delay_ms(400);
        }
        if (!gpio_read(input_bottom)) {
            // write : "MORE PIZZA "
            delay_ms(400);
        }
        if (!gpio_read(input_top)) {
            // write : "Dawson "
            delay_ms(400);
        }
        if (!gpio_read(input_right)) {
            // write : "minor "
            delay_ms(400);
        }
    }
}

void display_file(fat32_fs_t *fs, pi_dirent_t *directory, pi_dirent_t *file_dirent) {
    trace("about to display file %s\n", file_dirent->name);
    if (!file_dirent || file_dirent->is_dir_p) {
        // Not a valid file
        display_clear();
        display_write(10, 20, "Not a valid file", WHITE, BLACK, 1);
        display_update();
        delay_ms(1000);
        return;
    }

    // Read the file
    trace("attempt to read file\n", file_dirent->name);
    pi_file_t *file = fat32_read(fs, directory, file_dirent->name);
    if (!file) {
        display_clear();
        display_write(10, 20, "Error reading file", WHITE, BLACK, 1);
        display_update();
        delay_ms(1000);
        return;
    }
    trace("finished reading file\n", file_dirent->name);

    // Variables for scrolling through file
    int start_line = 0;
    int lines_per_screen = 7; // Number of lines that fit on screen
    int max_char_per_line = 21; // Maximum characters per line
    int max_lines = file->n_data / max_char_per_line + 1;

    // Buffer to hold the portion of text to display
    char display_buffer[lines_per_screen * (max_char_per_line + 1) + 1]; // +1 for newlines, +1 for null terminator

    while(1) {
        // Format text for display (line wrapping and paging)
        display_buffer[0] = '\0';
        int buf_pos = 0;
        int char_count = 0;
        int line_count = 0;
        
        // Start from the current viewing position
        for (int i = 0; i < file->n_data && line_count < lines_per_screen; i++) {
            char c = file->data[i];
            
            // Skip to the start_line
            if (i == 0 && start_line > 0) {
                int skipped_lines = 0;
                for (int j = 0; j < file->n_data && skipped_lines < start_line; j++) {
                    char_count++;
                    if (file->data[j] == '\n' || char_count >= max_char_per_line) {
                        char_count = 0;
                        skipped_lines++;
                    }
                }
                i = skipped_lines * max_char_per_line;
                if (i >= file->n_data) break;
                c = file->data[i];
            }
            
            // Add character to buffer
            if (c == '\r') continue; // Skip carriage returns
            
            if (c == '\n' || char_count >= max_char_per_line - 1) {
                // End of line or line wrapping
                if (c != '\n') {
                    display_buffer[buf_pos++] = c;
                }
                display_buffer[buf_pos++] = '\n';
                char_count = 0;
                line_count++;
            } else {
                display_buffer[buf_pos++] = c;
                char_count++;
            }
            
            // Ensure null termination
            display_buffer[buf_pos] = '\0';
        }

        // Display the text
        display_clear();
        
        // Show file name at the top
        display_write(0, 0, file_dirent->name, WHITE, BLACK, 1);
        
        // Draw a line separator
        display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
        
        // Show the file content
        display_write(0, 12, display_buffer, WHITE, BLACK, 1);
        
        // Show scroll indicators if needed
        if (start_line > 0) {
            display_write(SSD1306_WIDTH - 8, 0, "^", WHITE, BLACK, 1);
        }
        if (start_line + lines_per_screen < max_lines) {
            display_write(SSD1306_WIDTH - 8, SSD1306_HEIGHT - 8, "v", WHITE, BLACK, 1);
        }
        
        display_update();

        // Wait for button input
        delay_ms(100);
        
        // Check for button presses and scroll accordingly
        if (!gpio_read(input_top) && start_line > 0) {
            // Scroll up
            start_line--;
            delay_ms(200);
        }
        
        if (!gpio_read(input_bottom) && start_line + lines_per_screen < max_lines) {
            // Scroll down
            start_line++;
            delay_ms(200);
        }
        
        if (!gpio_read(input_left)) {
            // Exit file view
            break;
        }
    }
}

void show_files(fat32_fs_t *fs, pi_dirent_t *directory) {
    uint32_t num_entries_to_show = 4; // show 4 files at a time
    pi_directory_t files = fat32_readdir(fs, directory);
    unsigned num_entries_in_dir = files.ndirents;

    printk("Got %d files.\n", files.ndirents);
    for (int i = 0; i < files.ndirents; i++) {
        pi_dirent_t *dirent = &files.dirents[i];
        if (dirent->is_dir_p) {
        printk("\tD: %s (cluster %d)\n", dirent->name, dirent->cluster_id);
        } else {
        printk("\tF: %s (cluster %d; %d bytes)\n", dirent->name, dirent->cluster_id, dirent->nbytes);
        }
    }


    
    if (num_entries_in_dir == 0) {
        display_clear();
        display_write(10, 20, "No files found", WHITE, BLACK, 1);
        display_update();
        delay_ms(1000);
        return;
    }
    
    uint32_t top_index = 0;
    uint32_t bot_index = min(top_index + num_entries_to_show - 1, num_entries_in_dir - 1);
    uint32_t selected_index = 0; // Currently selected file index
    
    char text_to_display[18 * num_entries_to_show + 1]; // file name can be max 16 bytes long + 2 for enter, 1 at the end for null terminator

    while(1) {
        // Build the display text properly
        text_to_display[0] = '\0';
        int text_pos = 0;
        
        for (int i = top_index; i <= bot_index; i++) {
            pi_dirent_t *dirent = &files.dirents[i];
            
            // Add selection indicator for the currently selected file
            if (i == selected_index) {
                text_to_display[text_pos++] = '>';
            } else {
                text_to_display[text_pos++] = ' ';
            }
            
            // Copy the filename to buffer
            int j = 0;
            while (dirent->name[j] != '\0' && text_pos < sizeof(text_to_display) - 2) {
                text_to_display[text_pos++] = dirent->name[j++];
            }
            
            if (dirent->is_dir_p && text_pos < sizeof(text_to_display) - 2) {
                // Shift the content right by one character to make room for '/'
                for (int i = text_pos; i > 0; i--) {
                    text_to_display[i] = text_to_display[i-1];
                }
                // Add '/' at the beginning
                text_to_display[0] = '/';
                text_pos++; // Increment position counter for the added character
            }
            
            // Add newline if there's space
            if (text_pos < sizeof(text_to_display) - 2) {
                text_to_display[text_pos++] = '\n';
            }
            
            // Ensure null termination
            text_to_display[text_pos] = '\0';
        }
        
        display_clear();
        display_write(10, 0, "File Browser", WHITE, BLACK, 1);
        display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
        display_write(0, 12, text_to_display, WHITE, BLACK, 1);
        
        // Display navigation hints
        display_write(0, SSD1306_HEIGHT - 8, "^v:Move <:Back >:Open", WHITE, BLACK, 1);
        
        display_update();
        
        // Add a small delay to debounce buttons
        delay_ms(100);
        
        // Navigation logic
        if (!gpio_read(input_left)) {
            // Exit file browser
            trace("exiting file system");
            break;
        }
        
        if (!gpio_read(input_bottom)) {
            // Move selection down
            if (selected_index < num_entries_in_dir) {
                selected_index++;
                // If selection moves below visible area, scroll down
                if (selected_index > bot_index) {
                    top_index++;
                    bot_index = min(top_index + num_entries_to_show - 1, num_entries_in_dir - 1);
                }
            }
            delay_ms(200);
        }
        
        if (!gpio_read(input_right)) {
            // Open selected file or directory
            pi_dirent_t *selected_dirent = &files.dirents[selected_index];
            
            if (selected_dirent->is_dir_p) {
                // If it's a directory, navigate into it
                display_clear();
                display_write(10, 20, "Entering directory...", WHITE, BLACK, 1);
                display_update();
                delay_ms(500);
                
                // Recursive call to show files in the selected directory
                show_files(fs, selected_dirent);
            } else {
                // If it's a file, display its contents
                display_file(fs, directory, selected_dirent);
            }
            delay_ms(200);
        }

        if (!gpio_read(input_top)) {
            // Move selection up
            if (selected_index > 0) {
                selected_index--;
                // If selection moves above visible area, scroll up
                if (selected_index < top_index) {
                    top_index--;
                    bot_index = min(top_index + num_entries_to_show - 1, num_entries_in_dir - 1);
                }
            }
            delay_ms(200);
        }


        if (!gpio_read(input_single)) {
            // display menu to create file
            
        }
    }
}

void show_files(fat32_fs_t *fs, pi_dirent_t *directory) {

     // TODO

}

void notmain(void) {
    trace("Starting Button + Display Integration Test\n");

    // display init 
    trace("Initializing display at I2C address 0x%x\n", SSD1306_I2C_ADDR);
    display_init();
    trace("Display initialized\n");
    
    // Clear the display
    display_clear();
    display_update();
    trace("Display cleared\n");

    // button init
    trace("Initializing buttons\n");

    // do in the middle in case some interference
    gpio_set_input(input_single);
    gpio_set_input(input_right);
    gpio_set_input(input_bottom);
    gpio_set_input(input_top);
    gpio_set_input(input_left);
    trace("buttons initialized\n");
    //test_buttons();

    // Initialize FAT32 filesystem
    trace("Starting FAT shenanigans\n");
    kmalloc_init(FAT32_HEAP_MB);
    pi_sd_init();
  
    trace("Reading the MBR\n");
    mbr_t *mbr = mbr_read();
  
    trace("Loading the first partition\n");
    mbr_partition_ent_t partition;
    memcpy(&partition, mbr->part_tab1, sizeof(mbr_partition_ent_t));
    assert(mbr_part_is_fat32(partition.part_type));
  
    trace("Loading the FAT\n");
    fat32_fs_t fs = fat32_mk(&partition);
  
    trace("Loading the root directory\n");
    pi_dirent_t root = fat32_get_root(&fs);
  
    // Display welcome screen with bouncing Pi symbol animation
    int bounce_x = SSD1306_WIDTH / 2;
    int bounce_y = 30;
    int dx = 1;
    int dy = 1;
    int frame = 0;
    
    while (gpio_read(input_single) && gpio_read(input_right) && 
           gpio_read(input_bottom) && gpio_read(input_top) && 
           gpio_read(input_left)) {
        
        display_clear();
        
        display_write(8, 2, "SUZITI File Browser", WHITE, BLACK, 1);
        display_draw_line(0, 12, SSD1306_WIDTH, 12, WHITE);
        
        // Draw a cute <3 folder icon using primitive shapes
        display_fill_rect(bounce_x - 7, bounce_y - 4, 14, 10, WHITE);
        display_fill_rect(bounce_x - 5, bounce_y - 6, 10, 2, WHITE);
        display_fill_rect(bounce_x - 6, bounce_y - 3, 12, 8, BLACK);
        display_fill_rect(bounce_x - 4, bounce_y - 2, 8, 2, WHITE);
        display_fill_rect(bounce_x - 4, bounce_y + 1, 8, 2, WHITE);
        display_fill_rect(bounce_x - 4, bounce_y + 4, 8, 2, WHITE);
        
        if (frame % 30 < 15) {
            display_write(10, 48, "Press any button", WHITE, BLACK, 1);
        }
        display_update();
        
        // bouncing effect
        bounce_x += dx;
        bounce_y += dy;
        
        if (bounce_x <= 10 || bounce_x >= SSD1306_WIDTH - 10) {
            dx = -dx;
        }
        if (bounce_y <= 20 || bounce_y >= 40) {
            dy = -dy;
        }
        
        frame++;
        delay_ms(50);  // Control animation speed
    }
    
    delay_ms(200); // Debounce
    
    // Start file browser
    show_files(&fs, &root);
    
    // Exit message
    display_clear();
    display_write(10, 20, "File Browser Exited", WHITE, BLACK, 1);
    display_update();
    delay_ms(2000);
}