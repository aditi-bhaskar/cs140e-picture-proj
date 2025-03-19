# our notmain code:

PROGS= 1-test-fat-laptop.c
# PROGS= 1-test-fat-laptop-aditi.c
#PROGS= tests/2-fat32-ls.c


####################

# things to include

COMMON_SRC += mbr.c
COMMON_SRC += fat32.c

STAFF_OBJS += $(CS140E_2025_PATH)/libpi/staff-objs/kmalloc.o
STAFF_OBJS += staff-faster-i2c.o 

CFLAGS  = -Iexternal-code
COMMON_SRC += pi-sd.c mbr-helpers.c fat32-helpers.c fat32-lfn-helpers.c external-code/unicode-utf8.c external-code/emmc.c#  external-code/mbox.c 


#####################

# swap these if your bootloader doesn't work.
BOOTLOADER = my-install
BOOTLOADER = pi-install


L := $(CS140E_2025_PATH)/libpi/
# uncomment this if you want to use the staff
# STAFF_OBJS := $(L)/staff-objs/staff-sw-uart.o

# if doing checkoff use both uart and cstart.
ifdef CHECKOFF
    COMMON_SRC = sw-uart.c
    PROGS= $(wildcard [01234]-*.c)
endif

include $(CS140E_2025_PATH)/libpi/mk/Makefile.template-fixed

checkoff:
	make -C ./ CHECKOFF=1 check
	@echo "checking that not using staff uart: if you see output is an error:"
	arm-none-eabi-nm $(BUILD_DIR)/*.elf  | grep staff_uart
