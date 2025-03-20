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
char unique_folder_id = 65; // starts at: "A" // used when creating dirs
char unique_dup_id = 65; // starts at: "A" // used when duplicating files

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
const uint32_t NUM_ENTRIES_TO_SHOW = 5; // Show 4 files at a time on screen
uint32_t bot_index;
int file_pos = 0;

// for display_text
int current_start_line = 0;
int lines_per_screen = 5; 
const int CHAR_WIDTH = 6;  // Approximate width of a character in pixels
const int MAX_CHARS_PER_LINE = (SSD1306_WIDTH - 5) / CHAR_WIDTH;  // Max chars per line with margin


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


void copy_file_contents(fat32_fs_t *fs, pi_dirent_t *directory, char *origin_filename, char *dest_filename) {   
    display_clear();
    display_write(10, 0, "Appending to file", WHITE, BLACK, 1);
    display_write(10, 20, origin_filename, WHITE, BLACK, 1); // say which file
    display_write(10, 30, dest_filename, WHITE, BLACK, 1); // say which file
    // display_write(10, 40, append_me, WHITE, BLACK, 1); // say what's being appended
    display_update();

    // TODO show file here!!

    printk("\n\n\n\n\n>>!!! origin file name = %s\n\n", origin_filename);
    printk("\n\n\n\n\n\n>>!!! file name = %s\n\n", dest_filename);

    pi_file_t *file = fat32_read(fs, directory, origin_filename);
    printk("file -> data is %s\n\n", file->data);
    printk("writing to fat\n");
    int writ = fat32_write(fs, directory, dest_filename, file);

    delay_ms(600);
}


void dup_file(fat32_fs_t *fs, pi_dirent_t *directory, char *raw_name) { // the raw filename with the extension
    ls(fs, directory);
    
    pi_dirent_t *created_file = NULL;
    char filename[10] = {'D','U','P','E',unique_dup_id,'.','T','X','T','\0'};
    do {
        filename[4] = unique_dup_id; // make sure we create a new file!!
        created_file = fat32_create(fs, directory, filename, 0); // 0=not a directory
        unique_dup_id++; // if needed for the next loop/iteration
    } while (created_file == NULL);

    printk(">>DUPLICATING FILE %s", filename);
    delay_ms(2000);

    copy_file_contents(fs, directory, raw_name, filename); // add nothing to the file. maybe this adds a \0?? idc TODO fix?

    ls(fs, directory);

    delay_ms(400);
}


    
void append_to_file(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, char* append_me) {
    display_clear();
    display_write(10, 10, "Appending to file", WHITE, BLACK, 1);
    display_write(10, 30, filename, WHITE, BLACK, 1); // say which file
    display_write(10, 40, append_me, WHITE, BLACK, 1); // say what's being appended
    display_update();

    // TODO show file here!!
    printk(">>!!! file name = %s\n\n", filename);

    printk("Reading file\n");
    pi_file_t *file = fat32_read(fs, directory, filename);
    char *data_old = "file->data";

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


void show_menu(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {

    printk(" in show menu! \n\n");
    delay_ms(400);
    uint8_t selected_item = 1;
    uint8_t NUM_MENU_OPTIONS = 3; // cr f, cr d, dup f

    while(1) {

        display_clear();
        // meny 
        display_draw_char(6, selected_item*10, '>', WHITE, BLACK, 1);
        // display_draw_char(SSD1306_WIDTH - (6 * 2), selected_item*10, ')', WHITE, BLACK, 1);

        // menu options
        display_write(12, 10, "create file", WHITE, BLACK, 1);
        display_write(12, 20, "create dir ", WHITE, BLACK, 1);
        display_write(12, 30, "duplic file", WHITE, BLACK, 1);

        // control info
        display_draw_line(0, SSD1306_HEIGHT-(6 * 3)-1, SSD1306_WIDTH, SSD1306_HEIGHT-(6 * 3)-1, WHITE);
        display_write(2, SSD1306_HEIGHT-(6 * 3), "^v:Move >:Do *:Exit", WHITE, BLACK, 1);

        display_update();

        
        if (!gpio_read(input_right)) {
            // go into that menu option.
            switch (selected_item) {
                case 1: 
                    create_file(fs, directory);
                    break;
                case 2:
                    create_dir(fs, directory);
                    break;
                default:
                    dup_file(fs, directory, filename);
                    break;
            }
            delay_ms(200);
            return;
        }
        else if (!gpio_read(input_bottom)) {
            if (selected_item < NUM_MENU_OPTIONS) selected_item++;
            delay_ms(200);
        }
        else if (!gpio_read(input_top)) {
            if (selected_item > 1) selected_item--;
            delay_ms(200);
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


int split_text_up(int **line_positions_ptr, pi_file_t *file, int estimated_lines) {
    // Preprocess the file into screen-friendly lines
    // First, count approximately how many lines we'll need
    int *line_positions = *line_positions_ptr;
    // Allocate memory for line start positions
    int line_count = 0;
    
    // Process the entire file to find line breaks
    int pos = 0;
    int chars_in_line = 0;
    
    // Mark the start of the first line
    line_positions[line_count++] = 0;
    
    // Scan through the file to find line breaks
    while (pos < file->n_data && line_count < estimated_lines - 1) {
        char c = file->data[pos];
        
        // Skip carriage returns
        if (c == '\r') {
            pos++;
            continue;
        }
        
        // Handle existing newlines in the file
        if (c == '\n') {
            // Mark the start of a new line
            line_positions[line_count++] = pos + 1;
            chars_in_line = 0;
            pos++;
            continue;
        }
        
        chars_in_line++;
        
        // Check if we need to wrap this line - simply cut at the maximum character count
        if (chars_in_line >= MAX_CHARS_PER_LINE) {
            // Force a break at the current position, splitting words if necessary
            line_positions[line_count++] = pos + 1;  // Start new line at next character
            chars_in_line = 0;
        }
        
        pos++;
    }
    return line_count;
}

void determine_screen_content_file(char **text, size_t buffer_size, pi_file_t *file, int *line_positions, int line_count) {
    char *display_buffer = *text;
    // Generate the text content for current view
    display_buffer[0] = '\0';
    int buf_pos = 0;
    
    // Collect lines_per_screen lines starting from current_start_line
    for (int i = 0; i < lines_per_screen && (current_start_line + i) < line_count; i++) {
        int line_start = line_positions[current_start_line + i];
        int line_end;
        
        // Determine where this line ends
        if (current_start_line + i + 1 < line_count) {
            line_end = line_positions[current_start_line + i + 1];
            // If next position is after a newline, don't include the newline twice
            if (line_end > 0 && file->data[line_end-1] == '\n') {
                line_end--;
            }
        } else {
            line_end = file->n_data;
        }
        
        // Copy the line content to the display buffer
        for (int j = line_start; j < line_end && buf_pos < buffer_size - 2; j++) {
            char c = file->data[j];
            if (c != '\r') {  // Skip carriage returns
                display_buffer[buf_pos++] = c;
            }
        }
        
        // Add a newline if the line doesn't end with one already
        if (buf_pos > 0 && display_buffer[buf_pos-1] != '\n' && i < lines_per_screen - 1 && 
            buf_pos < buffer_size - 2) {
            display_buffer[buf_pos++] = '\n';
        }
    }
    
    // Ensure null termination
    display_buffer[buf_pos] = '\0';
}

void display_file_text(const char *filename, char **text, int line_count) {
    char *display_buffer = *text;
    // Display the content
    display_clear();
        
    // Show file name at the top
    display_write(10, 0, filename, WHITE, BLACK, 1);
    display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
    
    // Show the file content
    display_write(0, 12, display_buffer, WHITE, BLACK, 1);
    
    // Add scroll indicators if needed
    if (current_start_line > 0) {
        display_write(SSD1306_WIDTH - 8, 0, "^", WHITE, BLACK, 1);
    }
    
    if (current_start_line + lines_per_screen < line_count) {
        display_write(SSD1306_WIDTH - 8, SSD1306_HEIGHT - 8, "v", WHITE, BLACK, 1);
    }
    
    // Add navigation help at bottom
    display_write(0, SSD1306_HEIGHT - 8, "<:Back ^v:Scroll", WHITE, BLACK, 1);
    display_draw_line(0, SSD1306_HEIGHT - 10, SSD1306_WIDTH, SSD1306_HEIGHT - 10, WHITE);
    
    display_update();
}

void display_text(pi_file_t *file, const char *filename) {
    trace("about to display text file");

    int estimated_lines = (file->n_data / MAX_CHARS_PER_LINE) + file->n_data / 20 + 10;  // Overestimate to be safe
    int *line_positions = kmalloc(sizeof(int) * estimated_lines);
    int line_count = split_text_up(&line_positions, file, estimated_lines); // populates line_positions
    
    // Buffer for the displayed text
    char display_buffer[512];
    char *display_buffer_ptr = display_buffer;
    
    // Initialize the starting line index (used for scrolling)
    current_start_line = 0;
    
    // Main display loop
    while (1) {
        determine_screen_content_file(&display_buffer_ptr, 512, file, line_positions, line_count);

        display_file_text(filename, &display_buffer_ptr, line_count);
        
        // Wait for button press
        while (gpio_read(input_left) && gpio_read(input_right) && 
               gpio_read(input_top) && gpio_read(input_bottom) && 
               gpio_read(input_single)) {
            delay_ms(50);
        }
        
        if (!gpio_read(input_left)) { // back button
            return;
        }
        
        if (!gpio_read(input_top) && current_start_line > 0) {
            // Scroll up one line at a time
            current_start_line--;
            delay_ms(150);
        }
        
        if (!gpio_read(input_bottom) && (current_start_line + lines_per_screen) < line_count) {
            // Scroll down one line at a time
            current_start_line++;
            delay_ms(150);
        }
        
        delay_ms(200); // debouncing
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
        display_interactive_pbm(file, file_dirent->name);
    }
    else {
        display_text(file, file_dirent->name); // for displaying normal text file
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

void determine_screen_content_navigation(char **text, size_t buffer_size) {
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
        determine_screen_content_navigation(&text_ptr, 18 * NUM_ENTRIES_TO_SHOW + 1); // fills in text_to_display via text_ptr
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
            int real_selected_index = 0;
            int count = 0;
            for (int i = 0; i < total_entries; i++) {
                if (files.dirents[i].name[0] == '.') {
                    continue;  // Skip entries that start with .
                }
                if (count == selected_index) {
                    real_selected_index = i;
                    break;
                }
                count++;
            }

            // Get the selected directory entry
            pi_dirent_t *selected_dirent = &files.dirents[real_selected_index];
            
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
        }
        else if (!gpio_read(input_single)) {
            show_menu(&current_dir.entry, selected_dirent->name);//name of file);

            // TODO MAKE HELPER
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

            // // Get the selected directory entry
            // pi_dirent_t *selected_dirent = &files.dirents[real_selected_index];
            
            // if (selected_dirent->is_dir_p) {
            //     display_clear();
            //     display_write(10, 20, "Can't copy directory!!", WHITE, BLACK, 1);
            //     display_update();
            //     delay_ms(400);
            // } else {
            //     show_menu(fs, &current_dir.entry, selected_dirent->name);//name of file);
            //     delay_ms(200);
            // }
        
        }
        delay_ms(200); // Debounce
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