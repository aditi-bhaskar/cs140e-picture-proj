// TODO 
// 1. setup_directory_info (fills in global values), determine_screen_content (fills array), display_file_navigation
//      ^ use these functions and printk debug why newly created files/dirs/duplications won't show up as soon as they're created

// 3. save pbm edits to file!

// 4. test create folder + file rigorously

// 5. make duplicate file accessible; inside the text file

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
pi_dirent_t root; // root directory


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

void navigate_file_system(pi_dirent_t *directory);
void start_screen(void);
void setup_directory_info(pi_dirent_t *current_dir);
int show_pbm_menu(void);
void display_text(pi_dirent_t *directory, pi_file_t *file, const char *filename);
int split_text_up(int **line_positions_ptr, pi_file_t *file, int estimated_lines);
void determine_screen_content_file(char **text, size_t buffer_size, pi_file_t *file, int *line_positions, int line_count);
void dup_file(pi_dirent_t *directory, const char *origin_name) ;

//******************************************
// FUNCTIONS!!
//******************************************

void ls(pi_dirent_t *directory) {
    pi_directory_t files = fat32_readdir(&fs, directory);
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

//******************************************
// DUPLICATING A FILE
//******************************************

void show_file_dup_menu(pi_dirent_t *current_dir, pi_file_t *file, const char *filename) {
    printk(" in dup menu! \n\n");
    delay_ms(200);
    uint8_t selected_item = 1;
    uint8_t NUM_MENU_OPTIONS = 1; // dup file

    while(1) {
        display_clear();
        // menu selector
        display_draw_char(6, selected_item*10, '>', WHITE, BLACK, 1);

        // menu options
        display_write(12, 10, "duplicate file", WHITE, BLACK, 1);

        // control info
        display_draw_line(0, SSD1306_HEIGHT-(6 * 3)-1, SSD1306_WIDTH, SSD1306_HEIGHT-(6 * 3)-1, WHITE);
        display_write(2, SSD1306_HEIGHT-(6 * 3), "^v:Move >:Select *:Exit", WHITE, BLACK, 1);

        display_update();

        if (!gpio_read(input_right)) {
            // go into that menu option.
            switch (selected_item) {
                case 1: 
                    dup_file(current_dir, filename);
                    break;
                default:
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
            start_screen(); // go back to start screen
        }
    }
}


void copy_file_contents(fat32_fs_t *fs, pi_dirent_t *directory, const char *origin_filename, char *dest_filename) {   
    display_clear();
    display_write(10, 0, "Copying file", WHITE, BLACK, 1);
    display_write(10, 20, origin_filename, WHITE, BLACK, 1); // say which file
    display_write(10, 30, dest_filename, WHITE, BLACK, 1); // say which file
    display_update();

    char *mutable_orign_filename = (char *)origin_filename;

    pi_file_t *file = fat32_read(fs, directory, mutable_orign_filename);
    printk("writing to fat\n");
    int writ = fat32_write(fs, directory, dest_filename, file);

    delay_ms(600);
}

// TODO add functionality to call this
void dup_file(pi_dirent_t *directory, const char *origin_name) { // the raw filename with the extension
    ls(directory);
    
    pi_dirent_t *created_file = NULL;
    char new_filename[10] = {'D','U','P','E',unique_dup_id,'.','T','X','T','\0'};
    do {
        new_filename[4] = unique_dup_id; // make sure we create a new file!!
        created_file = fat32_create(&fs, directory, new_filename, 0); // 0=not a directory
        unique_dup_id++; // if needed for the next loop/iteration
    } while (created_file == NULL);

    printk(">>DUPLICATING FILE %s", new_filename);
    copy_file_contents(&fs, directory, origin_name, new_filename); 
    
    setup_directory_info(directory); // to set up any new files/dirs which might have been created
    ls(directory);

    delay_ms(400);
}

//******************************************
// CREATING FILES & DIRS
//******************************************

void display_file_text_while_appending(const char *filename, char **text, int line_count) {
    printk("displaying file while appending\n");
    char *display_buffer = *text;
    // clear display
    display_clear();
        
    // write file name at the top 
    display_write(10, 0, filename, WHITE, BLACK, 1);
    display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
    
    // write file content
    display_write(0, 12, display_buffer, WHITE, BLACK, 1);
    
    // add scroll indicators if needed
    if (current_start_line > 0) {
        display_write(SSD1306_WIDTH - 8, 0, "^", WHITE, BLACK, 1);
    }
    
    if (current_start_line + lines_per_screen < line_count) {
        display_write(SSD1306_WIDTH - 8, SSD1306_HEIGHT - 8, "v", WHITE, BLACK, 1);
    }
    
    // add navigation help at bottom
    display_write(0, SSD1306_HEIGHT - 8, "<^v>:Write *:Exit", WHITE, BLACK, 1);
    display_draw_line(0, SSD1306_HEIGHT - 10, SSD1306_WIDTH, SSD1306_HEIGHT - 10, WHITE);
    
    display_update();
}

void display_text_while_appending(pi_file_t *file, const char *filename) {
    trace("about to display text file\n");

    int estimated_lines = (file->n_data / MAX_CHARS_PER_LINE) + file->n_data / 20 + 10;  // Overestimate to be safe
    int *line_positions = kmalloc(sizeof(int) * estimated_lines);
    int line_count = split_text_up(&line_positions, file, estimated_lines); // populates line_positions
    
    // buf for displayed text
    char display_buffer[512];
    char *display_buffer_ptr = display_buffer;
    
    // init the starting line index (used for scrolling)
    current_start_line = 0;
    
    while ( (current_start_line + lines_per_screen) < line_count ) {          
        // Scroll down one line at a time
        current_start_line++;
    } 
    determine_screen_content_file(&display_buffer_ptr, 512, file, line_positions, line_count);
    display_file_text_while_appending(filename, &display_buffer_ptr, line_count);
    
}
    
void append_to_file(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, char* append_me) {

    printk("Reading file\n");
    pi_file_t *file = fat32_read(fs, directory, filename);

    if  (file == NULL) printk ("FILE NOT FOUND -- CANT READ IT!!");
    char *data_old = file->data;

    // write : "append me" to the file
    char data[strlen(data_old) + strlen(append_me)+ 1];
    data[strlen(data_old) + strlen(append_me)] = '\0';

    printk ("filename: %s\n", filename);
    printk("\n\n\nold data contents = %s\n", data_old);
    printk("append me contents = %s\n", append_me);
    strcpy(data, data_old);   // Copy old data
    strcat(data, append_me);  // Append new data
    printk("data contents = %s\n", data);

    pi_file_t new_file_contents = (pi_file_t) {
        .data = data,
        .n_data = strlen(data),
        .n_alloc = strlen(data),
    };

    printk("writing to fat\n");
    int writ = fat32_write(fs, directory, filename, &new_file_contents);

    printk("writing to screen\n");
    pi_file_t *file_read = fat32_read(fs, directory, filename);
    if (file_read == NULL) printk ("THE FILE IS NULL!!!\n");
    printk("fileread contents: %s", file_read->data);
    printk(".\n"); // incase prev line was empty spaces or smth
    display_text_while_appending(file_read, filename);    
}

void create_file(pi_dirent_t *directory) {
    ls(directory);

    // doesn't create file if file alr exists
    pi_dirent_t *created_file;
    // create a file; name it a random number like demoLETTER
    char filename[10] = {'D','E','M','O',unique_file_id,'.','T','X','T','\0'};
    do {
        filename[4] = unique_file_id++; // make sure we create a new file!!
        created_file = fat32_create(&fs, directory, filename, 0); // 0=not a directory
    } while (created_file == NULL);
    
    delay_ms(400);

    // display navigation hints
    display_clear();
    display_write(0, SSD1306_HEIGHT - 16, "<^v> : write to file", WHITE, BLACK, 1);
    display_write(0, SSD1306_HEIGHT - 8, "* : to exit", WHITE, BLACK, 1);
    display_update();
    while(1) {
        printk("in create_file, waiting\n");
        // exit file writing
        if (!gpio_read(input_single)) {
            // exit file creation return
            printk("broke out of create_file\n");
            delay_ms(200);
            break;
        }

        // these buttons write to file
        if (!gpio_read(input_left)) {
            append_to_file(&fs, directory, filename, "*prefetch flush* ");
        } else if (!gpio_read(input_bottom)) {
            append_to_file(&fs, directory, filename, "MORE PIZZA ");
        } else if (!gpio_read(input_right)) {
            append_to_file(&fs, directory, filename, "minor ");
        } else if (!gpio_read(input_top)) {
            append_to_file(&fs, directory, filename, "Dawson!! ");
        } 
        delay_ms(200);

    }
    setup_directory_info(directory); // to set up any new files/dirs which might have been created
    printk("AFTER CREATE FILE:\n");
    ls(directory);
}


uint8_t does_dir_exist(pi_dirent_t *directory, char *foldername) {

    pi_directory_t files = fat32_readdir(&fs, directory);
    printk("Got %d files.\n", files.ndirents);

    for (int i = 0; i < files.ndirents; i++) {
      pi_dirent_t *dirent = &files.dirents[i];
      if (dirent->is_dir_p) {
        if (strncmp(dirent->name, foldername, 4) == 0) {
            return 0; // bad!
        }
      }
    }
    return 1; // good!
}

void create_dir(pi_dirent_t *directory) {
    ls(directory);

    // create a foldername; name it with a random number like dirA
    char foldername[5] = {'D','I','R',unique_folder_id,'\0'};
    do {
        foldername[3] = unique_folder_id++; // make sure we create a new foldername!!
    } while (does_dir_exist(directory, foldername) == 0); // 0 means a dir with that name alr exists

    pi_dirent_t *created_folder = fat32_create(&fs, directory, foldername, 1); // 1 = create a directory

    display_clear();
    // Display navigation hints
    display_write(10, SSD1306_HEIGHT - 8*3, "creating dir", WHITE, BLACK, 1);
    display_write(10, SSD1306_HEIGHT - 8*2, foldername, WHITE, BLACK, 1);
    display_update();

    setup_directory_info(directory); // to set up any new files/dirs which might have been created
    printk("Created a directory!\n");
    ls(directory);

    delay_ms(2000);
}


//******************************************
// DRAWING AND PBMs
//******************************************

void display_show_drawing_status(int drawing_mode) {
    char status[20];
    snprintk(status, sizeof(status), "Mode: %s", 
             drawing_mode ? "DRAWING" : "NAVIGATE");
    
    // Display the status
    display_write(0, SSD1306_HEIGHT - 16, status, WHITE, BLACK, 1);
    display_draw_line(0, SSD1306_HEIGHT - 18, SSD1306_WIDTH, SSD1306_HEIGHT - 18, WHITE);
    
    // Add hint for menu
    display_write(0, SSD1306_HEIGHT - 8, "*:Menu ^v<>:Move", WHITE, BLACK, 1);
}

int show_pbm_menu(void) {
    uint8_t selected_item = 1;
    uint8_t NUM_MENU_OPTIONS = 3;

    while(1) {
        display_clear();
        
        display_write(10, 0, "PBM Edit Menu", WHITE, BLACK, 1);
        display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
        display_draw_char(6, selected_item*10 + 5, '>', WHITE, BLACK, 1);
        display_write(12, 15, "Navigate Mode", WHITE, BLACK, 1);
        display_write(12, 25, "Draw Mode", WHITE, BLACK, 1);
        display_write(12, 35, "Exit File", WHITE, BLACK, 1);

        display_draw_line(0, SSD1306_HEIGHT-9, SSD1306_WIDTH, SSD1306_HEIGHT-9, WHITE);
        display_write(0, SSD1306_HEIGHT-8, "^v:Move >:Select *:Back", WHITE, BLACK, 1);

        display_update();
        
        while (gpio_read(input_left) && gpio_read(input_right) && 
               gpio_read(input_top) && gpio_read(input_bottom) && 
               gpio_read(input_single)) {
            delay_ms(50);
        }
        
        if (!gpio_read(input_right)) {
            // select
            return selected_item;
        }
        else if (!gpio_read(input_bottom)) {
            if (selected_item < NUM_MENU_OPTIONS) selected_item++;
            delay_ms(150);
        }
        else if (!gpio_read(input_top)) {
            if (selected_item > 1) selected_item--;
            delay_ms(150);
        }
        else if (!gpio_read(input_single)) {
            // return to prev screen
            return 0;
        }
    }
}

/**
 * Display PBM file with editing capabilities
 */
void display_pbm(pi_file_t *file, const char *filename) {
    // display image + header info
    display_draw_pbm((uint8_t*)file->data, file->n_data);
    display_write(10, 0, filename, WHITE, BLACK, 1);
    display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);

    // copy display buf for edits
    static uint8_t edit_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
    memcpy(edit_buffer, buffer, sizeof(edit_buffer));
    
    // reset cursor and turn drawing mode off
    cursor_x = 0; 
    cursor_y = 12;
    drawing_mode = DRAWING_MODE_OFF;
    
    // bounds check for cursor
    int bottom_boundary = SSD1306_HEIGHT - 20; // margin to stay above status area
    
    // show status of drawing & the cursor
    display_show_drawing_status(drawing_mode);
    display_draw_cursor(cursor_x, cursor_y, drawing_mode);
    display_update();
    
    // i/o loop
    while(1) {
        delay_ms(100);
        
        int cursor_moved = 0;
        
        if (!gpio_read(input_left)) {
            // move left
            if (cursor_x > 0) {
                cursor_x -= cursor_speed;
                cursor_moved = 1;
            }
        }
        else if (!gpio_read(input_right)) {
            // move right
            if (cursor_x < SSD1306_WIDTH - 1) {
                cursor_x += cursor_speed;
                cursor_moved = 1;
            }
        }
        else if (!gpio_read(input_bottom)) {
            // move down 
            if (cursor_y < bottom_boundary) { // bounds check for bottom
                cursor_y += cursor_speed;
                cursor_moved = 1;
            }
        }
        else if (!gpio_read(input_top)) {
            // move up
            if (cursor_y > 10) { // bounds check for header
                cursor_y -= cursor_speed;  
                cursor_moved = 1;
            }
        }
        else if (!gpio_read(input_single)) {
            // show menu
            delay_ms(200);
            int choice = show_pbm_menu();
            switch(choice) {
                case 1: //  navigate mode (not drawing, but can move cursor)
                    drawing_mode = DRAWING_MODE_OFF;
                    break;
                    
                case 2: // enter draw mode
                    drawing_mode = DRAWING_MODE_ON;
                    break;
                    
                case 3: // save and quit file
                    // TODO SAVE FILE!!
                    return;
                    
                default:
                    break;
            }
            
            // redraw screen after menu selection
            memcpy(buffer, edit_buffer, sizeof(buffer));
            display_show_drawing_status(drawing_mode);
            display_draw_cursor(cursor_x, cursor_y, drawing_mode);
            display_update();
            
            delay_ms(150);
            continue; // skip to next iteration and dont check other if statements below
        }
        
        // cursor moved while in drawing mode --> draw a pixel
        if (cursor_moved && drawing_mode) {
            uint16_t byte_idx = cursor_x + (cursor_y / 8) * SSD1306_WIDTH;
            uint8_t bit_pos = cursor_y % 8;
            edit_buffer[byte_idx] |= (1 << bit_pos); // while pixel
        }
        
        // only redraw cursor position if cursor position changed
        if (cursor_moved) {
            // restore the buffer from our edit buffer
            memcpy(buffer, edit_buffer, sizeof(buffer));
            
            // redraw the header again
            display_show_drawing_status(drawing_mode);
            
            // redraw cursor at new pos
            display_draw_cursor(cursor_x, cursor_y, drawing_mode);
            display_update();
        }
    }
}

//******************************************
// FILE NAVIGATION AND MENU
//******************************************

void show_filesystem_menu(pi_dirent_t *current_dir) {
    printk(" in show menu! \n\n");
    delay_ms(200);
    uint8_t selected_item = 1;
    uint8_t NUM_MENU_OPTIONS = 2; // cr f, cr d, dup f

    while(1) {

        display_clear();
        // menu selector
        display_draw_char(6, selected_item*10, '>', WHITE, BLACK, 1);
        // menu options
        display_write(12, 10, "create file", WHITE, BLACK, 1);
        display_write(12, 20, "create directory ", WHITE, BLACK, 1);
        // control info
        display_draw_line(0, SSD1306_HEIGHT-(6 * 3)-1, SSD1306_WIDTH, SSD1306_HEIGHT-(6 * 3)-1, WHITE);
        display_write(2, SSD1306_HEIGHT-(6 * 3), "^v:Move >:Select *:Exit", WHITE, BLACK, 1);
        display_update();

        if (!gpio_read(input_right)) {
            // go into that menu option.
            switch (selected_item) {
                case 1: 
                    create_file(current_dir);
                    break;
                case 2:
                    create_dir(current_dir);
                    break;
                default:
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
            navigate_file_system(current_dir);
            printk("SETTING UP THE DIR INFO!! ON EXITING CREATE F/D\n\n");
            setup_directory_info(current_dir); // to set up any new files/dirs which might have been created
        }
    }

}

int split_text_up(int **line_positions_ptr, pi_file_t *file, int estimated_lines) {

    // count num lines
    int *line_positions = *line_positions_ptr;
    // alloc for line start positions
    int line_count = 0;
    
    // find linebreaks in file
    int pos = 0;
    int chars_in_line = 0;
    
    // mark start of the first line
    line_positions[line_count++] = 0;
    
    // scan thru file to find line breaks
    while (pos < file->n_data && line_count < estimated_lines - 1) {
        char c = file->data[pos];
        
        // skip \r carriage returns
        if (c == '\r') {
            pos++;
            continue;
        }
        
        // handle \n
        if (c == '\n') {
            // Mark the start of a new line
            line_positions[line_count++] = pos + 1;
            chars_in_line = 0;
            pos++;
            continue;
        }
        
        chars_in_line++;
        
        // check if we need to wrap this line - simply cut at the maximum character count
        if (chars_in_line >= MAX_CHARS_PER_LINE) {
            // force break at cur pos, spliting words if needed
            line_positions[line_count++] = pos + 1;  // start new line at next character
            chars_in_line = 0;
        }
        
        pos++;
    }
    return line_count;
}

void determine_screen_content_file(char **text, size_t buffer_size, pi_file_t *file, int *line_positions, int line_count) {
    
    printk("in determine_screen_content_file\n");

    char *display_buffer = *text;
    display_buffer[0] = '\0';
    int buf_pos = 0;
    int lines_displayed = 0;  // track how many actual lines we've displayed
    
    for (int i = 0; i < lines_per_screen && (current_start_line + i) < line_count; i++) {
        int line_start = line_positions[current_start_line + i];
        int line_end;
        
        if (current_start_line + i + 1 < line_count) {
            line_end = line_positions[current_start_line + i + 1];
        } else {
            line_end = file->n_data;
        }
        
        // copy line content, including any newlines
        for (int j = line_start; j < line_end && buf_pos < buffer_size - 2; j++) {
            char c = file->data[j];
            if (c != '\r') {  // skip \r = carriage returns
                display_buffer[buf_pos++] = c;
                if (c == '\n') {
                    lines_displayed++;
                }
            }
        }
        
        // of the line didn't end with a newline and it's not the last line we're displaying,
        // add a newline
        if ((buf_pos == 0 || display_buffer[buf_pos-1] != '\n') && 
            i < lines_per_screen - 1 && buf_pos < buffer_size - 2) {
            display_buffer[buf_pos++] = '\n';
            lines_displayed++;
        }
    }
    
    // null termination!!
    display_buffer[buf_pos] = '\0';
}


//******************************************
// FILE & DIR READING
//******************************************

void display_file_text(const char *filename, char **text, int line_count) {
    char *display_buffer = *text;
    // clear display
    display_clear();
        
    // write file name at the top 
    display_write(10, 0, filename, WHITE, BLACK, 1);
    display_draw_line(0, 10, SSD1306_WIDTH, 10, WHITE);
    
    // write file content
    display_write(0, 12, display_buffer, WHITE, BLACK, 1);
    
    // add scroll indicators if needed
    if (current_start_line > 0) {
        display_write(SSD1306_WIDTH - 8, 0, "^", WHITE, BLACK, 1);
    }
    
    if (current_start_line + lines_per_screen < line_count) {
        display_write(SSD1306_WIDTH - 8, SSD1306_HEIGHT - 8, "v", WHITE, BLACK, 1);
    }
    
    // add navigation help at bottom
    display_write(0, SSD1306_HEIGHT - 8, "<:Back ^v:Scroll", WHITE, BLACK, 1);
    display_draw_line(0, SSD1306_HEIGHT - 10, SSD1306_WIDTH, SSD1306_HEIGHT - 10, WHITE);
    
    display_update();
}

void display_text(pi_dirent_t *directory, pi_file_t *file, const char *filename) {
    trace("about to display text file\n");

    int estimated_lines = (file->n_data / MAX_CHARS_PER_LINE) + file->n_data / 20 + 10;  // Overestimate to be safe
    int *line_positions = kmalloc(sizeof(int) * estimated_lines);
    int line_count = split_text_up(&line_positions, file, estimated_lines); // populates line_positions
    
    // buf for displayed text
    char display_buffer[512];
    char *display_buffer_ptr = display_buffer;
    
    // init the starting line index (used for scrolling)
    current_start_line = 0;
    
    // display loop
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

        if (!gpio_read(input_single)) {
            show_file_dup_menu(directory, file, filename);
            delay_ms(150);
        }
        
        delay_ms(200); // debouncing
    }
}

void display_something(fat32_fs_t *fs, pi_dirent_t *directory, pi_dirent_t *file_dirent) {
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
        display_pbm(file, file_dirent->name);
    }
    else {
        display_text(directory, file, file_dirent->name); // for displaying normal text file
    }
}

void setup_directory_info(pi_dirent_t *current_dir_entry) {
    trace("entering setup_directory_info\n");
    selected_index = 0;  // Reset selection
    top_index = 0;       // Reset view position
    // Read current directory contents; this setup happens everywhere
    files = fat32_readdir(&fs, current_dir_entry);
    total_entries = files.ndirents;
    trace("Got %d files.\n", files.ndirents);
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
    ls(current_dir_entry); // debug stuff

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



void navigate_file_system(pi_dirent_t *starting_directory) {
    trace("entered navigate_file_system \n");
    // Create an extended directory structure to track parents
    ext_dirent_t current_dir;
    current_dir.entry = *starting_directory;
    current_dir.parent = NULL;  // Root has no parent
    setup_directory_info(&current_dir.entry);

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
            setup_directory_info(&(current_dir.entry));

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

            // get selected directory entry
            pi_dirent_t *selected_dirent = &files.dirents[real_selected_index];
            
            if (selected_dirent->is_dir_p) {
                // create a new extended directory entry with parent pointer
                ext_dirent_t* new_parent = kmalloc(sizeof(ext_dirent_t));

                // copy the current directory to be used as parent
                *new_parent = current_dir;

                // create a new current directory with the selected entry
                current_dir.entry = *selected_dirent;
                current_dir.parent = new_parent;
                setup_directory_info(&current_dir.entry);
                
                // Regular directory - navigate into it
                display_clear();
                display_write(10, 20, "Entering directory...", WHITE, BLACK, 1);
                display_update();
                delay_ms(200);
            } 
            else {
                // It's a file - display its content
                display_something(&fs, &current_dir.entry, selected_dirent);
            }
        }
        else if (!gpio_read(input_bottom)) { // scroll down
            if (selected_index < adjusted_total - 1) {
                selected_index++;
                
                // scroll if selection goes below visible area
                if (selected_index > bot_index) {
                    top_index++;
                }
            }
        }
        else if (!gpio_read(input_top)) { // scroll up
            // move selection up
            if (selected_index > 0) {
                selected_index--;
                
                // scroll if selection goes above visible area
                if (selected_index < top_index) {
                    top_index--;
                }
            }
        }
        else if (!gpio_read(input_single)) {
            show_filesystem_menu(&(current_dir.entry)); // we don't need parent directory information
        }
        delay_ms(200); // debounce
    }
}


void start_screen(void) {
    // display welcome screen with bouncing ? symbol animation
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
            navigate_file_system(&root);
            delay_ms(200); // to make sure we don't immediately click into it
        }
        
        display_clear();
        
        display_write(8, 2, "SUZITI File System", WHITE, BLACK, 1);
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


//******************************************
// NOTMAIN
//******************************************

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
    trace("Finished loading the root directory\n");
  
    // shows cute start screen stuff
    start_screen();
    // we should never return from here since there is a while (1) loop
}