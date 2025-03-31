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
  pi_directory_t dir = fat32_readdir(&fs, &root);

  for (int i = 0; i < 200; i++) {
    char fname[10];
    memcpy(fname, "FOO___.TXT", 11); // note: filenames have to be capitalized
    fname[5] = '0' + (i % 10);
    fname[4] = '0' + ((i/10) % 10);
    fname[3] = '0' + ((i/100) % 10);

    printk("Creating file %s\n", fname);
    fat32_delete(&fs, &root, fname);
    assert(fat32_create(&fs, &root, fname, 0));
  }


  printk("PASS: %s\n", __FILE__);
}
