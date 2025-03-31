#include "rpi.h"
#include "pi-sd.h"
#include "fat32.h"

void notmain() {
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

  printk("Looking for HELLO.TXT\n");
  pi_dirent_t *hello = fat32_stat(&fs, &root, "HELLO.TXT");

  if (hello) {
    printk("Found HELLO.TXT!\n");
  } else {
    panic("Didn't find HELLO.TXT Run write test to make it!\n");
  }
  printk("Truncating <%s> from %d bytes to 5\n", hello, hello->nbytes);
  if (!fat32_truncate(&fs, &root, "HELLO.TXT", 5)) {
    panic("Unable to truncate file!\n");
  }
  printk("PASS: %s\n", __FILE__);
}