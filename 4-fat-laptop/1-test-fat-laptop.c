// TODO 
// remove files / more on menu
#include "display.c"

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
char unique_folder_id = 65; // starts at: "A" // used when creating files

// for all functions; this should never change
fat32_fs_t fs;
pi_dirent_t root;


// for navigate_file_system
uint32_t top_index = 0;      // First visible entry index
uint32_t selected_index = 0;  // Currently selected entry
int entries_offset = 0;       // Offset for skipping special directories
uint32_t adjusted_total = 0;
uint32_t total_entries = 0;
pi_directory_t files;
const uint32_t NUM_ENTRIES_TO_SHOW = 4; // Show 4 files at a time on screen
uint32_t bot_index;

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

//******************************************
// FUNCTION HEADERS!!
//******************************************

void navigate_file_system(fat32_fs_t *fs, pi_dirent_t *directory);
void start_screen(fat32_fs_t fs, pi_dirent_t root);
//******************************************
// FUNCTIONS!!
//******************************************


void ls(fat32_fs_t *fs, pi_dirent_t *directory) {
    pi_directory_t files = fat32_readdir(fs, directory);
    printk("Got %d files.\n", files.ndirents);

    for (int i = 0; i < files.ndirents; i++) {
      pi_dirent_t *dirent = &files.dirents[i];
      if (dirent->is_dir_p) {
        printk("\tD: %s (cluster %d)\n", dirent->name, dirent->cluster_id);
      } else {
        printk("\tF: %s (cluster %d; %d bytes)\n", dirent->name, dirent->cluster_id, dirent->nbytes);
      }
    }
}


void append_to_file(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, char* append_me) {
    display_clear();
    display_write(10, 10, "Appending to file", WHITE, BLACK, 1);
    display_write(10, 30, filename, WHITE, BLACK, 1); // say which file
    display_write(10, 40, append_me, WHITE, BLACK, 1); // say what's being appended
    display_update();

    // TODO show file here!!
    
    printk("Reading file\n");
    pi_file_t *file = fat32_read(fs, directory, filename);
    char *data_old = file->data;

    // write : "append me" to the file
    char data[strlen(data_old) + strlen(append_me)+ 1];
    strcpy(data, data_old);   // Copy old data
    strcat(data, append_me);  // Append new data
    printk("data contents = %s\n\n", data);

    pi_file_t new_file_contents = (pi_file_t) {
        .data = data,
        .n_data = strlen(data),
        .n_alloc = strlen(data),
    };

    printk("writing to fat\n");

    int writ = fat32_write(fs, directory, filename, &new_file_contents);
    printk("wrote *prefetch flush* to file: %s\n", filename);
    
    delay_ms(600);

}

void create_dir(fat32_fs_t *fs, pi_dirent_t *directory) {
    ls(fs, directory);

    // doesn't create file if file alr exists
    pi_dirent_t *created_folder = NULL;
    // create a file; name it a random number like dirA
    printk("\n\n\n creating a dir \n\n\n");
    char foldername[5] = {'D','I','R',unique_folder_id,'\0'};
    do {
        printk("\n\n\n in dowhile \n\n\n");

        created_folder = fat32_create(fs, directory, foldername, 1); // 1 = create a directory
        foldername[3] = unique_folder_id++; // make sure we create a new file!!

    } while (created_folder == NULL);

    display_clear();
    // Display navigation hints
    display_write(10, SSD1306_HEIGHT - 8*3, "creating dir", WHITE, BLACK, 1);
    display_write(10, SSD1306_HEIGHT - 8*2, foldername, WHITE, BLACK, 1);
    display_update();

    delay_ms(2000);
}

void create_file(fat32_fs_t *fs, pi_dirent_t *directory) {
    ls(fs, directory);

    // doesn't create file if file alr exists
    pi_dirent_t *created_file;
    // create a file; name it a random number like demoLETTER
    char filename[10] = {'D','E','M','O',unique_file_id,'.','T','X','T','\0'};
    do {
        created_file = fat32_create(fs, directory, filename, 0); // 0=not a directory
        filename[4] = unique_file_id++; // make sure we create a new file!!
    } while (created_file == NULL);
    
    delay_ms(400);

    while(1) {
        printk("in create_file, waiting\n");
        display_clear();
        // Display navigation hints
        display_write(0, SSD1306_HEIGHT - 16, "<^v> : write to file", WHITE, BLACK, 1);
        display_write(0, SSD1306_HEIGHT - 8, "* : to exit", WHITE, BLACK, 1);
        display_update();

        // exit file writing
        if (!gpio_read(input_single)) {
            // exit file creation return
            printk("broke out of create_file\n");
            delay_ms(200);
            break;
        }

        // TODO display this stuff on screen ??

        // these buttons write to file
        if (!gpio_read(input_left)) {
            append_to_file(fs, directory, filename, "*prefetch flush* ");
        } else if (!gpio_read(input_bottom)) {
            append_to_file(fs, directory, filename, "MORE PIZZA ");
        } else if (!gpio_read(input_right)) {
            append_to_file(fs, directory, filename, "minor ");
        } else if (!gpio_read(input_top)) {
            append_to_file(fs, directory, filename, "Dawson!! ");
        } 
        delay_ms(200);

    }
}


void display_interactive_pbm(pi_file_t *file, const char *filename) {
    
    // Initial display of the image
    display_draw_pbm((uint8_t*)file->data, file->n_data);
    
    // Show file name at the top
    display_write(10, 0, filename, WHITE, BLACK, 1);
    display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
    
    // Reset cursor and drawing mode
    cursor_x = 0;
    cursor_y = 12;  // Just below the header
    drawing_mode = DRAWING_MODE_OFF;
    
    // Initial drawing status
    display_show_drawing_status(drawing_mode);
    
    // Draw initial cursor
    display_draw_cursor(cursor_x, cursor_y, drawing_mode);
    display_update();
    
    // Control loop
    while(1) {
        // Wait for a button press
        // while(gpio_read(input_left) && gpio_read(input_right) && 
        //       gpio_read(input_top) && gpio_read(input_bottom) && 
        //       gpio_read(input_single)) {
        //     delay_ms(20);
        // }
        delay_ms(100); // maybe helps?
        
        // Process button inputs
        if (!gpio_read(input_left)) {
            if (drawing_mode == DRAWING_MODE_OFF) {
                // in navigate mode: Exit image view
                delay_ms(200); // Debounce
                return;
            } else {
                // Move cursor left in drawing mode
                if (cursor_x > 0) cursor_x -= cursor_speed;
            }
        }
        else if (!gpio_read(input_right)) {
            // Move cursor right
            if (cursor_x < SSD1306_WIDTH - 1) cursor_x += cursor_speed;
        }
        else if (!gpio_read(input_top)) {
            // Move cursor up
            if (cursor_y > 10) cursor_y -= cursor_speed;  // Stay below header
        }
        else if (!gpio_read(input_bottom)) {
            // Move cursor down
            if (cursor_y < SSD1306_HEIGHT - 17) cursor_y += cursor_speed; // Stay above status
        }
        else if (!gpio_read(input_single)) {
            drawing_mode = !drawing_mode;
            display_show_drawing_status(drawing_mode);
            
            // Debounce
            //delay_ms(150);
        }
        
        // Update cursor
        display_draw_cursor(cursor_x, cursor_y, drawing_mode);
        display_update();
        
        // Wait for button release
        // while(!gpio_read(input_left) || !gpio_read(input_right) || 
        //       !gpio_read(input_top) || !gpio_read(input_bottom) || 
        //       !gpio_read(input_single)) {
        //     delay_ms(20);
        // }
    }
}


void show_menu(fat32_fs_t *fs, pi_dirent_t *directory) {

    printk(" in show menu! \n\n");
    delay_ms(400);
    uint8_t selected_item = 1;
    uint8_t NUM_MENU_OPTIONS = 2;

    while(1) {

        display_clear();
        // meny 
        display_draw_char(128 - (6 * 3), selected_item*10, '(', WHITE, BLACK, 1);
        display_draw_char(128 - (6 * 1), selected_item*10, ')', WHITE, BLACK, 1);

        // menu options
        display_draw_char(128 - (6 * 2), 10, 'f', WHITE, BLACK, 1);
        display_draw_char(128 - (6 * 2), 20, 'd', WHITE, BLACK, 1);

        display_write(10, 2, "menu\n>: select, \n*: exit", WHITE, BLACK, 1);

        display_update();

        
        if (!gpio_read(input_right)) {
            // go into that menu option.
            switch (selected_item) {
                case 1: 
                    create_file(fs, directory);
                    break;
                case 2:
                    printk("gonna create a dir");
                    create_dir(fs, directory);
                    break;
                default:
                    printk("didn't enter any cases");
                    break;
            }
            delay_ms(200);
            return;
        }
        else if (!gpio_read(input_bottom)) {
            if (selected_item < NUM_MENU_OPTIONS) selected_item++;
        }
        else if (!gpio_read(input_top)) {
            if (selected_item > 1) selected_item--;
        }
        else if (!gpio_read(input_single)) {
            // close menu
            display_clear();
            display_update();
            navigate_file_system(fs, directory);
        }
        

        // TODO implm up/down arrows to select other menu options; highlight them
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

    // Check if this is a PBM file
    if (is_pbm_file(file_dirent->name)) {
        // Display PBM image with interactive drawing capability
        trace("trying to display PBM with drawing capability\n");
        display_interactive_pbm(file, file_dirent->name);
        return; // Return directly after interactive mode
    }
    else {
        // For text files, proceed with regular text viewing...
        
        // Variables for scrolling through file
        int start_line = 0;
        int lines_per_screen = 10;      // Show 10 lines on screen
        int max_lines = file->n_data;   // Maximum number of lines to scroll through

        // Buffer to hold the portion of text to display
        char display_buffer[512]; // Large enough for the visible portion

        while(1) {
            // Format text for display
            display_buffer[0] = '\0';
            int buf_pos = 0;
            int line_count = 0;
            
            // Calculate the actual position in the file to start from
            int file_pos = 0;
            int current_line = 0;
            
            // Skip to starting line
            while (current_line < start_line && file_pos < file->n_data) {
                if (file->data[file_pos] == '\n') {
                    current_line++;
                }
                file_pos++;
            }
            
            // Now read lines from the calculated position
            for (int i = file_pos; i < file->n_data && line_count < lines_per_screen; i++) {
                char c = file->data[i];
                
                // Add character to buffer
                if (c == '\r') continue; // Skip carriage returns
                
                display_buffer[buf_pos++] = c;
                
                // Ensure null termination
                display_buffer[buf_pos] = '\0';
                
                // Count lines
                if (c == '\n') {
                    line_count++;
                }
                
                // Check if we've completed all lines
                if (line_count >= lines_per_screen) break;
            }

            // Display the text
            display_clear();
            
            // Show file name at the top
            display_write(10, 0, file_dirent->name, WHITE, BLACK, 1);
            display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
            
            // Show the file content
            display_write(0, 12, display_buffer, WHITE, BLACK, 1);
            
            // Add scroll indicators if needed
            if (start_line > 0) {
                display_write(SSD1306_WIDTH - 8, 0, "^", WHITE, BLACK, 1);
            }
            
            // Estimate if there's more content below
            if (file_pos + strlen(display_buffer) < file->n_data) {
                display_write(SSD1306_WIDTH - 8, SSD1306_HEIGHT - 8, "v", WHITE, BLACK, 1);
            }
            
            // Add navigation help at bottom
            display_write(0, SSD1306_HEIGHT - 8, "<:Back ^v:Scroll", WHITE, BLACK, 1);
            
            display_update();
            trace("contents of display buffer: %s", display_buffer);

            while(gpio_read(input_left) && gpio_read(input_right) && 
                  gpio_read(input_top) && gpio_read(input_bottom) && 
                  gpio_read(input_single)) {
                // Just wait for a button press
                delay_ms(50);
            }
            
            // Check for button presses and scroll accordingly
            if (!gpio_read(input_left)) {
                // Exit file view immediately
                return;
            }
            
            if (!gpio_read(input_top) && start_line > 0) {
                // Scroll up one line at a time
                start_line--;
                delay_ms(200);
            }
            
            if (!gpio_read(input_bottom) && file_pos + strlen(display_buffer) < file->n_data) {
                // Scroll down one line at a time
                start_line++;
                delay_ms(200);
            }
             
            if (!gpio_read(input_right) && file_pos + strlen(display_buffer) < file->n_data) {
                // Also scroll down one line at a time
                start_line++;
                delay_ms(200);
            }
            
            // Wait for button release
            while(!gpio_read(input_left) || !gpio_read(input_right) || 
                  !gpio_read(input_top) || !gpio_read(input_bottom) || 
                  !gpio_read(input_single)) {
                delay_ms(50);
            }
        }
    }
}

void setup_directory_info(ext_dirent_t *current_dir) {
    selected_index = 0;  // Reset selection
    top_index = 0;       // Reset view position
    // Read current directory contents; this setup happens everywhere
    files = fat32_readdir(&fs, &(current_dir->entry));
    total_entries = files.ndirents;
    printk("Got %d files.\n", files.ndirents);
    // Count how many entries start with . to calculate our offset (bc we don't show them)
    entries_offset = 0;
    for (int i = 0; i < total_entries; i++) {
        pi_dirent_t *dirent = &files.dirents[i];
        if (dirent->name[0] == '.') {
            // Skip entries that start with . bc they are funky and somehow refer to cluster 0
            entries_offset++;
        }
    }
    adjusted_total = total_entries - entries_offset;
    ls(&fs, &(current_dir->entry)); // debug stuff

    // Handle empty directories (after filtering)
    if (adjusted_total == 0) { // TODO: is this in the right location?? suze
        display_clear();
        display_write(10, 20, "Empty directory", WHITE, BLACK, 1);
        display_update();
        delay_ms(1000);
    }
    
    // Reset selection when entering a new directory --> do we still need this?
    if (selected_index >= adjusted_total) {
        selected_index = 0;
        top_index = 0;
    }
}

void determine_what_to_show(char **text, size_t buffer_size) {
    // determining what to show on screen (which files etc.)
    bot_index = min(top_index + NUM_ENTRIES_TO_SHOW - 1, adjusted_total - 1);
    char *text_to_display = *text;
        
    // Build the display text
    text_to_display[0] = '\0';
    int text_pos = 0;
    
    // Add visible entries to the display buffer
    int real_index = 0;  // Index into the actual directory entries
    int filtered_index = 0;  // Index into filtered entries (excluding '.' entries)
    // determines which files to show
    while (filtered_index <= bot_index && real_index < total_entries) {
        pi_dirent_t *dirent = &files.dirents[real_index];
        
        // Skip entries that start with . (as before)
        if (dirent->name[0] == '.') {
            real_index++;
            continue;
        }
        
        // Only process entries that are within our visible range
        if (filtered_index >= top_index && filtered_index <= bot_index) {
            // Add selection indicator
            if (filtered_index == selected_index) {
                text_to_display[text_pos++] = '>';
            } else {
                text_to_display[text_pos++] = ' ';
            }
            
            // If it's a directory, add the slash
            if (dirent->is_dir_p) {
                text_to_display[text_pos++] = '/';
            }
            
            // Copy filename
            int j = 0;
            // Skip trailing spaces when displaying
            int last_non_space = -1;
            while (dirent->name[j] != '\0' && text_pos < buffer_size - 2) {
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
            if (text_pos < buffer_size - 2) {
                text_to_display[text_pos++] = '\n';
            }
            
            text_to_display[text_pos] = '\0';
        }
        filtered_index++;
        real_index++;
    }
    printk("what will be shown: %s", text_to_display);
}

void display_file_navigation(ext_dirent_t *current_dir, char **text) {
    display_clear();

    // show directory name at the top
    char dir_name[22]; // Limit to screen width
    if (current_dir->entry.name[0] == '\0') {
        safe_strcpy(dir_name, "Root Directory", sizeof(dir_name));
    } else {
        safe_strcpy(dir_name, current_dir->entry.name, sizeof(dir_name));
    }
    display_write(10, 0, dir_name, WHITE, BLACK, 1);
    display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
    
    // Show file list
    display_write(0, 12, (*text), WHITE, BLACK, 1);
    display_write(0, SSD1306_HEIGHT - 8, "^v:Move <:Back >:Open", WHITE, BLACK, 1);

    display_draw_line(0, SSD1306_HEIGHT - 10, SSD1306_WIDTH, SSD1306_HEIGHT - 10, WHITE);
    display_update();
    delay_ms(200); // debounce
}



void navigate_file_system(fat32_fs_t *fs, pi_dirent_t *starting_directory) {
    // Create an extended directory structure to track parents
    ext_dirent_t current_dir;
    current_dir.entry = *starting_directory;
    current_dir.parent = NULL;  // Root has no parent
    setup_directory_info(&current_dir);

    char text_to_display[18 * NUM_ENTRIES_TO_SHOW + 1]; // Max filename(16) + selector(1) + newline(1) + null(1)
    char *text_ptr = text_to_display; // bc arrays hate me in C 
        
    
    // Main file browser loop
    while(1) {
        determine_what_to_show(&text_ptr, 18 * NUM_ENTRIES_TO_SHOW + 1); // fills in text_to_display via text_ptr

        display_file_navigation(&current_dir, &text_ptr);

        // Wait for button input
        while(gpio_read(input_left) && gpio_read(input_right) && 
              gpio_read(input_top) && gpio_read(input_bottom) && 
              gpio_read(input_single)) {
            delay_ms(50);
        }
        
        if (!gpio_read(input_left)) { // back button
            // return if parent is root
            if (current_dir.parent == NULL) {
                return;
            }
            // parent is not root; show text to indicate going back
            display_clear();
            display_write(10, 20, "Going back...", WHITE, BLACK, 1);
            display_update();
            delay_ms(200);
            
            // Navigate to parent directory
            ext_dirent_t* parent_dir = (ext_dirent_t*)current_dir.parent;
            current_dir = *parent_dir;
            setup_directory_info(&current_dir);

            delay_ms(200); // Debounce
        }
        else if (!gpio_read(input_right)) {
            // unsure if i still need this
            // int real_selected_index = 0;
            // int count = 0;
            // for (int i = 0; i < total_entries; i++) {
            //     if (files.dirents[i].name[0] == '.') {
            //         continue;  // Skip entries that start with .
            //     }
            //     if (count == selected_index) {
            //         real_selected_index = i;
            //         break;
            //     }
            //     count++;
            // }

            // Get the selected directory entry
            pi_dirent_t *selected_dirent = &files.dirents[selected_index];
            
            if (selected_dirent->is_dir_p) {
                // Create a new extended directory entry with parent pointer
                ext_dirent_t* new_parent = kmalloc(sizeof(ext_dirent_t));
                
                // Copy the current directory to be used as parent
                *new_parent = current_dir;
                
                // Create a new current directory with the selected entry
                current_dir.entry = *selected_dirent;
                current_dir.parent = new_parent;
                setup_directory_info(&current_dir);
                
                
                // Regular directory - navigate into it
                display_clear();
                display_write(10, 20, "Entering directory...", WHITE, BLACK, 1);
                display_update();
                delay_ms(200);
            } 
            else {
                // It's a file - display its content
                display_file(fs, &current_dir.entry, selected_dirent);
            }
            delay_ms(200); // Debounce
        }
        else if (!gpio_read(input_bottom)) { // scroll down
            if (selected_index < adjusted_total - 1) {
                selected_index++;
                
                // Scroll if selection goes below visible area
                if (selected_index > bot_index) {
                    top_index++;
                }
            }
            delay_ms(150); // Prevent rapid scrolling
        }
        else if (!gpio_read(input_top)) { // scroll up
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
        else if (!gpio_read(input_single)) {
            show_menu(fs, &current_dir.entry);
            delay_ms(200); // Debounce
        }
    }
}




void start_screen(fat32_fs_t fs, pi_dirent_t root) {
    // Display welcome screen with bouncing ? symbol animation
    int bounce_x = SSD1306_WIDTH / 2;
    int bounce_y = 30;
    int dx = 1;
    int dy = 1;
    int frame = 0;
    
    while (1) {
        if(!gpio_read(input_left) || !gpio_read(input_right) || 
              !gpio_read(input_top) || !gpio_read(input_bottom) || 
              !gpio_read(input_single)) {
                // someone pressed a button
            navigate_file_system(&fs, &root);
            delay_ms(200); // to make sure we don't immediately click into it
        }
        
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
}




void notmain(void) {
    trace("Initializing display at I2C address 0x%x\n", SSD1306_I2C_ADDR);
    display_init();
    trace("Display initialized\n");
    display_clear();
    display_update();

    gpio_set_input(input_single);
    gpio_set_input(input_right);
    gpio_set_input(input_bottom);
    gpio_set_input(input_top);
    gpio_set_input(input_left);
    //test_buttons();
    trace("buttons initialized\n");

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
    fs = fat32_mk(&partition);
    trace("Loading the root directory\n");
    root = fat32_get_root(&fs);
  
    // shows cute start screen stuff
    start_screen(fs, root);
    // we should never return from here since there is a while (1) loop
}