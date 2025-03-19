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

void show_files(fat32_fs_t *fs, pi_dirent_t *starting_directory) {
    // Constants for navigation
    const uint32_t NUM_ENTRIES_TO_SHOW = 4; // Show 4 files at a time on screen
    
    // Create a copy of the starting directory to keep track of where we are
    pi_dirent_t current_directory = *starting_directory;
    
    // Navigation state variables
    uint32_t top_index = 0;      // First visible entry index
    uint32_t selected_index = 0;  // Currently selected entry
    
    // Main file browser loop
    while(1) {
        // Read current directory contents
        pi_directory_t files = fat32_readdir(fs, &current_directory);
        uint32_t total_entries = files.ndirents;

        printk("Got %d files in directory: %s (cluster %d)\n", files.ndirents, 
               current_directory.name, current_directory.cluster_id);
        
        // DEBUG: Let's inspect all entries and look for special directories
        for (int i = 0; i < files.ndirents; i++) {
            pi_dirent_t *dirent = &files.dirents[i];
            
            // Deep debug for "..." entry
            if (dirent->name[0] == '.' && dirent->name[1] == '.' && dirent->name[2] == '.') {
                printk("\n---- DEBUG: '...' ENTRY FOUND ----\n");
                printk("  Index: %d\n", i);
                printk("  Name: '%s'\n", dirent->name);
                printk("  Name bytes: [%d][%d][%d][%d]\n", 
                      dirent->name[0], dirent->name[1], dirent->name[2], dirent->name[3]);
                printk("  Cluster ID: %d\n", dirent->cluster_id);
                printk("  Bytes: %d\n", dirent->nbytes);
                printk("  is_dir_p before: %d\n", dirent->is_dir_p);
                
                // Force it to be a directory
                dirent->is_dir_p = 1;
                
                // Trim any trailing spaces
                int j = 3;
                while(dirent->name[j] == ' ' && j < 8) { // FAT filenames can be up to 8 chars
                    dirent->name[j] = '\0';
                    j++;
                }
                
                printk("  is_dir_p after: %d\n");
                printk("  Name after trimming: '%s'\n", dirent->name);
                printk("  Current dir cluster: %d\n", current_directory.cluster_id);
                printk("---------------------------------\n\n");
            }
            // Debug for ".." entry as well
            else if (dirent->name[0] == '.' && dirent->name[1] == '.' && 
                    (dirent->name[2] == '\0' || dirent->name[2] == ' ')) {
                printk("\n---- DEBUG: '..' ENTRY FOUND ----\n");
                printk("  Index: %d\n", i);
                printk("  Name: '%s'\n", dirent->name);
                printk("  Cluster ID: %d\n", dirent->cluster_id);
                printk("  Bytes: %d\n", dirent->nbytes);
                printk("  is_dir_p: %d\n", dirent->is_dir_p);
                printk("  Current dir cluster: %d\n", current_directory.cluster_id);
                printk("---------------------------------\n\n");
                
                // Force it to be a directory
                dirent->is_dir_p = 1;
                // Trim any trailing spaces
                if (dirent->name[2] == ' ') {
                    dirent->name[2] = '\0';
                }
            }
            
            if (dirent->is_dir_p) {
                printk("\tD: %s (cluster %d)\n", dirent->name, dirent->cluster_id);
            } else {
                printk("\tF: %s (cluster %d; %d bytes)\n", dirent->name, dirent->cluster_id, dirent->nbytes);
            }
        }
        
        // Handle empty directories
        if (total_entries == 0) {
            display_clear();
            display_write(10, 20, "Empty directory", WHITE, BLACK, 1);
            display_update();
            delay_ms(1000);
            continue;
        }
        
        // Reset selection when entering a new directory
        if (selected_index >= total_entries) {
            selected_index = 0;
            top_index = 0;
        }
        
        // Calculate visible range
        uint32_t bot_index = min(top_index + NUM_ENTRIES_TO_SHOW - 1, total_entries - 1);
        
        // Build the display text
        char text_to_display[18 * NUM_ENTRIES_TO_SHOW + 1]; // Max filename(16) + selector(1) + newline(1) + null(1)
        text_to_display[0] = '\0';
        int text_pos = 0;
        
        // Add visible entries to the display buffer
        for (int display_index = top_index; display_index <= bot_index; display_index++) {
            pi_dirent_t *dirent = &files.dirents[display_index];
            
            // Add selection indicator
            if (display_index == selected_index) {
                text_to_display[text_pos++] = '>';
            } else {
                text_to_display[text_pos++] = ' ';
            }
            
            // Check for special directory by examining the characters
            int is_special_dir = 0;
            
            // Check for ".." and "..." patterns and treat them as directories
            if ((dirent->name[0] == '.' && dirent->name[1] == '.' && 
                 (dirent->name[2] == '\0' || dirent->name[2] == ' ')) ||
                (dirent->name[0] == '.' && dirent->name[1] == '.' && dirent->name[2] == '.' &&
                 (dirent->name[3] == '\0' || dirent->name[3] == ' '))) {
                is_special_dir = 1; 
            }
            
            // If it's a directory or special directory, add the slash
            if (dirent->is_dir_p || is_special_dir) {
                text_to_display[text_pos++] = '/';
            }
            
            // Copy filename
            int j = 0;
            // Skip trailing spaces when displaying
            int last_non_space = -1;
            while (dirent->name[j] != '\0' && text_pos < sizeof(text_to_display) - 2) {
                if (dirent->name[j] != ' ') {
                    last_non_space = j;
                }
                text_to_display[text_pos++] = dirent->name[j++];
            }
            
            // Trim trailing spaces in display
            if (last_non_space >= 0 && j > 0) {
                text_pos = text_pos - (j - last_non_space - 1);
            }
            
            // Add newline
            if (text_pos < sizeof(text_to_display) - 2) {
                text_to_display[text_pos++] = '\n';
            }
            
            // Ensure null termination
            text_to_display[text_pos] = '\0';
        }
        
        // Display current directory name and file list
        display_clear();
        
        // Prepare directory name display
        char dir_name[22]; // Limit to screen width
        if (current_directory.name[0] == '\0') {
            safe_strcpy(dir_name, "Root Directory", sizeof(dir_name));
        } else {
            safe_strcpy(dir_name, current_directory.name, sizeof(dir_name));
        }
        
        // Show the current directory at the top
        display_write(10, 0, dir_name, WHITE, BLACK, 1);
        display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
        
        // Show file list
        display_write(0, 12, text_to_display, WHITE, BLACK, 1);
        
        // Show navigation help
        display_write(0, SSD1306_HEIGHT - 8, "^v:Move <:Back >:Open", WHITE, BLACK, 1);
        
        display_update();
        delay_ms(100); // Debouncing delay
        
        // Wait for button input
        while(gpio_read(input_left) && gpio_read(input_right) && 
              gpio_read(input_top) && gpio_read(input_bottom) && 
              gpio_read(input_single)) {
            // Just wait for a button press
            delay_ms(50);
        }
        
        // Handle button input
        if (!gpio_read(input_left)) {
            // This is the "back" button - let's navigate to parent directory
            
            // Find the "..." entry for going back (since we're treating "..." as parent directory)
            int parent_index = -1;
            for (int i = 0; i < total_entries; i++) {
                pi_dirent_t *dir_entry = &files.dirents[i];
                // Check for "..." (with or without trailing space)
                if (dir_entry->name[0] == '.' && dir_entry->name[1] == '.' && dir_entry->name[2] == '.' &&
                    (dir_entry->name[3] == '\0' || dir_entry->name[3] == ' ')) {
                    parent_index = i;
                    
                    printk("\n== BACK BUTTON: Selected '...' entry for navigation ==\n");
                    printk("  Index: %d\n", i);
                    printk("  Name: '%s'\n", dir_entry->name);
                    printk("  Cluster ID: %d\n", dir_entry->cluster_id);
                    printk("  is_dir_p: %d\n", dir_entry->is_dir_p);
                    
                    // Force it to be a directory
                    files.dirents[i].is_dir_p = 1;
                    // Trim any trailing spaces
                    if (files.dirents[i].name[3] == ' ') {
                        files.dirents[i].name[3] = '\0';
                    }
                    
                    printk("  Name after trimming: '%s'\n", files.dirents[i].name);
                    printk("  Current dir cluster: %d\n", current_directory.cluster_id);
                    printk("  Attempting to navigate to cluster: %d\n", files.dirents[i].cluster_id);
                    
                    // Test if we have a potential issue with cluster 0
                    if (files.dirents[i].cluster_id == 0) {
                        printk("  WARNING: Cluster ID is 0, which might cause navigation issues!\n");
                        
                        // Try to find a ".." entry as fallback
                        int fallback_index = -1;
                        for (int j = 0; j < total_entries; j++) {
                            if (j != i && // Skip the current "..." entry
                                files.dirents[j].name[0] == '.' && 
                                files.dirents[j].name[1] == '.' && 
                                (files.dirents[j].name[2] == '\0' || files.dirents[j].name[2] == ' ')) {
                                fallback_index = j;
                                printk("  FALLBACK: Found '..' entry at index %d with cluster %d\n", 
                                      j, files.dirents[j].cluster_id);
                                break;
                            }
                        }
                        
                        if (fallback_index != -1) {
                            printk("  Using '..' entry as fallback for navigation\n");
                            parent_index = fallback_index;
                        } else {
                            printk("  No fallback entry found.\n");
                        }
                    }
                    
                    printk("================================================\n\n");
                    break;
                }
            }
            
            // If no "..." entry found, try to find a ".." entry as fallback
            if (parent_index == -1) {
                printk("\n== BACK BUTTON: No '...' entry found, looking for '..' as fallback ==\n");
                
                for (int i = 0; i < total_entries; i++) {
                    pi_dirent_t *dir_entry = &files.dirents[i];
                    if (dir_entry->name[0] == '.' && dir_entry->name[1] == '.' && 
                        (dir_entry->name[2] == '\0' || dir_entry->name[2] == ' ')) {
                        parent_index = i;
                        
                        printk("  Fallback found at index %d\n", i);
                        printk("  Name: '%s'\n", dir_entry->name);
                        printk("  Cluster ID: %d\n", dir_entry->cluster_id);
                        printk("  is_dir_p: %d\n", dir_entry->is_dir_p);
                        
                        // Force it to be a directory
                        files.dirents[i].is_dir_p = 1;
                        
                        printk("================================================\n\n");
                        break;
                    }
                }
            }
            
            if (parent_index != -1) {
                // Navigate to parent directory using the chosen entry
                display_clear();
                display_write(10, 20, "Going up...", WHITE, BLACK, 1);
                display_update();
                delay_ms(200);
                
                printk("NAVIGATION: Attempting to navigate using directory entry at index %d\n", parent_index);
                printk("  Target cluster: %d\n", files.dirents[parent_index].cluster_id);
                
                current_directory = files.dirents[parent_index];
                selected_index = 0;  // Reset selection when going up
                top_index = 0;       // Reset view position
            } else {
                // If no navigation entry found (at root), exit file browser
                trace("exiting file system");
                return;
            }
            delay_ms(200); // Debounce
        }
        else if (!gpio_read(input_bottom)) {
            // Move selection down
            if (selected_index < total_entries - 1) {
                selected_index++;
                
                // Scroll if selection goes below visible area
                if (selected_index > bot_index) {
                    top_index++;
                }
            }
            delay_ms(200); // Prevent rapid scrolling
        }
        else if (!gpio_read(input_top)) {
            // Move selection up
            if (selected_index > 0) {
                selected_index--;
                
                // Scroll if selection goes above visible area
                if (selected_index < top_index) {
                    top_index--;
                }
            }
            delay_ms(200); // Prevent rapid scrolling
        }
        else if (!gpio_read(input_right)) {
            // Get the selected directory entry
            pi_dirent_t *selected_dirent = &files.dirents[selected_index];
            
            // CUSTOM BEHAVIOR: Use ".." to refresh current directory (like ".")
            if (selected_dirent->name[0] == '.' && selected_dirent->name[1] == '.' && 
                (selected_dirent->name[2] == '\0' || selected_dirent->name[2] == ' ')) {
                selected_dirent->is_dir_p = 1;
                
                // ".." selected - STAY in current directory (like ".")
                display_clear();
                display_write(10, 20, "Refreshing directory...", WHITE, BLACK, 1);
                display_update();
                delay_ms(200);
                // Simply continue the loop - don't change directory
                continue;
            }
            // CUSTOM BEHAVIOR: Use "..." to navigate to parent (like "..")
            else if (selected_dirent->name[0] == '.' && selected_dirent->name[1] == '.' && 
                     selected_dirent->name[2] == '.' &&
                     (selected_dirent->name[3] == '\0' || selected_dirent->name[3] == ' ')) {
                selected_dirent->is_dir_p = 1;
                
                // Trim any trailing spaces
                if (selected_dirent->name[3] == ' ') {
                    selected_dirent->name[3] = '\0';
                }
                
                printk("\n== RIGHT BUTTON: Selected '...' entry for navigation ==\n");
                printk("  Index: %d\n", selected_index);
                printk("  Name: '%s'\n", selected_dirent->name);
                printk("  Cluster ID: %d\n", selected_dirent->cluster_id);
                printk("  is_dir_p: %d\n", selected_dirent->is_dir_p);
                printk("  Current dir cluster: %d\n", current_directory.cluster_id);
                
                // Check for cluster 0 issue
                if (selected_dirent->cluster_id == 0) {
                    printk("  WARNING: Cluster ID is 0, which might cause navigation issues!\n");
                    
                    // Try to find a ".." entry to check its cluster
                    for (int i = 0; i < total_entries; i++) {
                        pi_dirent_t *dir_entry = &files.dirents[i];
                        if (dir_entry->name[0] == '.' && dir_entry->name[1] == '.' && 
                            (dir_entry->name[2] == '\0' || dir_entry->name[2] == ' ')) {
                            printk("  INFO: '..' entry found at index %d with cluster %d\n", 
                                  i, dir_entry->cluster_id);
                            break;
                        }
                    }
                }
                
                printk("================================================\n\n");
                
                // "..." selected - go to parent directory
                display_clear();
                display_write(10, 20, "Going to parent...", WHITE, BLACK, 1);
                display_update();
                delay_ms(200);
                
                current_directory = *selected_dirent;
                selected_index = 0;  // Reset selection
                top_index = 0;       // Reset view position
                continue;
            }
            
            // Handle selection action based on entry type (for regular directories/files)
            if (selected_dirent->is_dir_p) {
                // Regular directory - navigate into it
                display_clear();
                display_write(10, 20, "Entering directory...", WHITE, BLACK, 1);
                display_update();
                delay_ms(200);
                
                current_directory = *selected_dirent;
                selected_index = 0;  // Reset selection for new directory
                top_index = 0;       // Reset view position
            } 
            else {
                // It's a file - display its content
                display_file(fs, &current_directory, selected_dirent);
            }
            delay_ms(200); // Debounce
        }
        else if (!gpio_read(input_single)) {
            // Create a new file in current directory
            display_clear();
            display_write(5, 20, "Creating new file...", WHITE, BLACK, 1);
            display_update();
            delay_ms(500);
            
            create_file(fs, &current_directory, files);
            
            delay_ms(200); // Debounce
        }
    }
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