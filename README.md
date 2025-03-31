## SUZITI File System
The SUZITI File System is an implementation of the FAT32 file system for the Rasberry Pi (r/pi). We connect it with a small graphical display and 5 buttons to let a user navigate through the filesystem on the SD card of the r/pi, create files/directories and view/edit text and images; all with our own implementation of I2C and gorgeous UI!


### Code:
All relevant code is under 5-fat-laptop-i2c/ (lower numbered folders represent earlier of our implementation process). 

Our final "make"-able code is in 5-fat-laptop-i2c/. The "not-main" (aka main) file is 1-test-fat-laptop.c. All other folders hold different stages of the project.

### Features:
 - (Device) Display. added a new device to r/pi; a display! (SSD1306)
 - (Device/Hardware) Button Matrix. soldered buttons to communicate with display and connected with r/pi
 - (Hardware): created CAD “laptop” print
 - (Device) replaced staff I2C with our own version 
see 5-fat-laptop-i2c/bitbang-i2c.c
 - (OS) Fat32 writes!
   - Added support for directory creation in fat32.c
   - Added file and directory creation/writing on display
   - Added support to duplicate files with their content
   - See 5-fat-laptop-i2c/fat32.c
 - (OS) We can display the contents of and write to .txt and .pbm files 
   - We can write to .txt files on creation
   - We can display and draw on .pbm files - and save our edits!!
 - (UI) Created intuitive and beautiful display 
   - E.g. navigation that doesn’t fall off the screen, displays text in .txt files well 
   - Scroll through file and display where file is being edited (for txt and pbm)


### Team Contributions:
 - Aditi: Led hardware design, i2c bitbanged, file and dir create/write/duplicate
 - Suze: Led fat32, file navigation, pbm & txt open & edit, ui + design
 - Together: Wrote display, built/soldered "laptop", fat32

### Citations:
 - display based on adafruit code examples (in folder in repo)
 - got symbol displayed on home screen somewhere on reddit
