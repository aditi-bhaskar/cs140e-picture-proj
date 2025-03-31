
PROGS= 1-test-pad.c

###################

# NOTE THAT GPIO.C IS A LAB FOR THE COURSE
# SO ONLY THE OBJECT FILE IS INCLUDED HERE
#  **** DO NOT MAKE CLEAN!!! ****
# worst case, there is a copy of gpio.o in the top level o_copies directory

# swap these if your bootloader doesn't work.
# BOOTLOADER = my-install
BOOTLOADER = pi-install


L := $(CS140E_2025_PATH)/libpi/

include $(CS140E_2025_PATH)/libpi/mk/Makefile.template-fixed

checkoff:
	make -C ./ CHECKOFF=1 check
	@echo "checking that not using staff uart: if you see output is an error:"
	arm-none-eabi-nm $(BUILD_DIR)/*.elf  | grep staff_uart