#include "rpi.h"
#include "fat32.h"
#include "fat32-helpers.h"
#include "pi-sd.h"

// Print extra tracing info when this is enabled.  You can and should add your
// own.
static int trace_p = 1; 
static int init_p = 0;

fat32_boot_sec_t boot_sector;

fat32_fs_t fat32_mk(mbr_partition_ent_t *partition) {
  demand(!init_p, "the fat32 module is already in use\n");
  // TODO: Read the boot sector (of the partition) off the SD card.
  fat32_boot_sec_t *boot_sec = pi_sec_read(partition->lba_start, 1);
  boot_sector = *(fat32_boot_sec_t*)boot_sec;


  // TODO: Verify the boot sector (also called the volume id, `fat32_volume_id_check`)
  fat32_volume_id_check(boot_sec);
  
  // TODO: Read the FS info sector (the sector immediately following the boot
  // sector) and check it (`fat32_fsinfo_check`, `fat32_fsinfo_print`)
  assert(boot_sector.info_sec_num == 1);

  struct fsinfo *fsinfo_sector = pi_sec_read(partition->lba_start + 1, 1);
  
  fat32_fsinfo_check(fsinfo_sector);

  //fat32_fsinfo_print("fsinfo: ", fsinfo_sector);
  //fat32_fsinfo_print("boot: ", boot_sector);
  

  // END OF PART 2
  // The rest of this is for Part 3:

  // TODO: calculate the fat32_fs_t metadata, which we'll need to return.
  unsigned lba_start = partition->lba_start; // from the partition
  unsigned fat_begin_lba = lba_start + boot_sector.reserved_area_nsec; // the start LBA + the number of reserved sectors
  unsigned cluster_begin_lba = fat_begin_lba + boot_sector.nfats * boot_sector.nsec_per_fat; // the beginning of the FAT, plus the combined length of all the FATs
  unsigned sec_per_cluster = boot_sector.sec_per_cluster; // from the boot sector
  unsigned root_first_cluster = boot_sector.first_cluster; // from the boot sector
  unsigned n_entries = boot_sector.nsec_per_fat * boot_sector.bytes_per_sec / 4; // each entry is 4 bytes


  /*
   * TODO: Read in the entire fat (one copy: worth reading in the second and
   * comparing).
   *
   * The disk is divided into clusters. The number of sectors per
   * cluster is given in the boot sector byte 13. <sec_per_cluster>
   *
   * The File Allocation Table has one entry per cluster. This entry
   * uses 12, 16 or 28 bits for FAT12, FAT16 and FAT32.
   *
   * Store the FAT in a heap-allocated array.
   */
  uint32_t *fat;
  fat = pi_sec_read(fat_begin_lba, boot_sector.nsec_per_fat);

  // Create the FAT32 FS struct with all the metadata
  fat32_fs_t fs = (fat32_fs_t) {
    .lba_start = lba_start,
      .fat_begin_lba = fat_begin_lba,
      .cluster_begin_lba = cluster_begin_lba,
      .sectors_per_cluster = sec_per_cluster,
      .root_dir_first_cluster = root_first_cluster,
      .fat = fat,
      .n_entries = n_entries,
  };

  if (trace_p) {
    trace("begin lba = %d\n", fs.fat_begin_lba);
    trace("cluster begin lba = %d\n", fs.cluster_begin_lba);
    trace("sectors per cluster = %d\n", fs.sectors_per_cluster);
    trace("root dir first cluster = %d\n", fs.root_dir_first_cluster);
  }

  init_p = 1;
  return fs;
}

// Given cluster_number, get lba.  Helper function.
static uint32_t cluster_to_lba(fat32_fs_t *fs, uint32_t cluster_num) {
  assert(cluster_num >= 2);
  // TODO: calculate LBA from cluster number, cluster_begin_lba, and
  // sectors_per_cluster

  unsigned lba = fs->cluster_begin_lba + (cluster_num - 2) * fs->sectors_per_cluster;  // formula in fat32.h
  if (trace_p) trace("cluster %d to lba: %d\n", cluster_num, lba);
  return lba;
}

pi_dirent_t fat32_get_root(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // TODO: return the information corresponding to the root directory (just
  // cluster_id, in this case)
  return (pi_dirent_t) {
    .name = "",
      .raw_name = "",
      .cluster_id = fs->root_dir_first_cluster, // fix this
      .is_dir_p = 1,
      .nbytes = 0,
  };
}

// Given the starting cluster index, get the length of the chain.  Helper
// function.
static uint32_t get_cluster_chain_length(fat32_fs_t *fs, uint32_t start_cluster) {
  // TODO: Walk the cluster chain in the FAT until you see a cluster where
  // ⁠ fat32_fat_entry_type(cluster) == LAST_CLUSTER ⁠.  Count the number of
  // clusters.

  if (start_cluster == 0) { // for empty file; TODO: 
    return 0;
  }

  unsigned num_clusters = 0; // SOPHIA says this is 1???
  uint32_t cluster = start_cluster;
  while (fat32_fat_entry_type(cluster) != LAST_CLUSTER) {
    num_clusters++;
    cluster = fs->fat[cluster];
    // Maybe do a printk to check the cluster numbers
  }
  return num_clusters;
  
  // return 0;
}

// Given the starting cluster index, read a cluster chain into a contiguous
// buffer.  Assume the provided buffer is large enough for the whole chain.
// Helper function.
static void read_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data) {
  // TODO: Walk the cluster chain in the FAT until you see a cluster where
  // fat32_fat_entry_type(cluster) == LAST_CLUSTER.  For each cluster, copy it
  // to the buffer (⁠ data ⁠).  Be sure to offset your data pointer by the
  // appropriate amount each time.
  if (start_cluster == 0) {
    // empty file
    return;
  }

  int cur_cluster = start_cluster;
  while (fat32_fat_entry_type(cur_cluster) != LAST_CLUSTER) {   
    pi_sd_read(data, cluster_to_lba(fs, cur_cluster), fs->sectors_per_cluster);
    data += fs->sectors_per_cluster * boot_sector.bytes_per_sec;
    cur_cluster = fs->fat[cur_cluster];
  }
}

// Converts a fat32 internal dirent into a generic one suitable for use outside
// this driver.
static pi_dirent_t dirent_convert(fat32_dirent_t *d) {
  pi_dirent_t e = {
    .cluster_id = fat32_cluster_id(d),
    .is_dir_p = d->attr == FAT32_DIR,
    .nbytes = d->file_nbytes,
  };
  // can compare this name
  memcpy(e.raw_name, d->filename, sizeof d->filename);
  // for printing.
  fat32_dirent_name(d,e.name);
  return e;
}

// Gets all the dirents of a directory which starts at cluster `cluster_start`.
// Return a heap-allocated array of dirents.
static fat32_dirent_t *get_dirents(fat32_fs_t *fs, uint32_t cluster_start, uint32_t *dir_n) {
  // TODO: figure out the length of the cluster chain (see
  // `get_cluster_chain_length`)
  uint32_t cluster_length = get_cluster_chain_length(fs, cluster_start);
  //trace("cluster length SUZE: %d\n", cluster_length);

  // TODO: allocate a buffer large enough to hold the whole directory
  //trace("malloc size: %x\n", cluster_length * fs->sectors_per_cluster * boot_sector.bytes_per_sec);
  //trace("bytes per sec: %x\n",boot_sector.bytes_per_sec);
  void *dir_buffer = kmalloc(cluster_length * fs->sectors_per_cluster * boot_sector.bytes_per_sec);
  //trace("testtesttest\n");

  // TODO: read in the whole directory (see `read_cluster_chain`)
  read_cluster_chain(fs, cluster_start, dir_buffer);

  *dir_n = cluster_length * fs->sectors_per_cluster * boot_sector.bytes_per_sec / sizeof(fat32_dirent_t); // sizeof(fat32_dirent_t) = size of one directory entry
  
  return (fat32_dirent_t *)dir_buffer;
}

pi_directory_t fat32_readdir(fat32_fs_t *fs, pi_dirent_t *dirent) {
  demand(init_p, "fat32 not initialized!");
  demand(dirent->is_dir_p, "tried to readdir a file!");
  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, dirent->cluster_id, &n_dirents);

  // TODO: allocate space to store the pi_dirent_t return values
  pi_dirent_t *pi_dirent_t_buffer = kmalloc(n_dirents * sizeof(pi_dirent_t));

  // TODO: iterate over the directory and create pi_dirent_ts for every valid
  // file.  Don't include empty dirents, LFNs, or Volume IDs.  You can use
  // `dirent_convert`.
  uint32_t n_dirents_valid = 0;
  for (int i = 0; i < n_dirents; i++) {
    // when traversing all directory entries, don't copy these out.
    if (fat32_dirent_free(&dirents[i])) continue; // free space
    if (fat32_dirent_is_lfn(&dirents[i])) continue; // LFN version of name
    if (dirents[i].attr & FAT32_VOLUME_LABEL) continue; // volume label

    pi_dirent_t_buffer[n_dirents_valid] = dirent_convert(&dirents[i]); 
    n_dirents_valid++;
  }

  // TODO: create a pi_directory_t using the dirents and the number of valid
  // dirents we found
  return (pi_directory_t) {
    .dirents = pi_dirent_t_buffer,
    .ndirents = n_dirents_valid, // !!!! should this be 0 or n_dirents
  };
}

static int find_dirent_with_name(fat32_dirent_t *dirents, int n, char *filename) {
  // TODO: iterate through the dirents, looking for a file which matches the
  // name; use ⁠ fat32_dirent_name ⁠ to convert the internal name format to a
  // normal string.
  for (int i=0; i<n; i++) {
    char name[15]; // max bytes for short filename (also hardcoded for 11 in fat32_dirent_t)
    fat32_dirent_name(&dirents[i], name);
    if (strcmp(name, filename) == 0) return i;
  }
  return -1;
}

pi_dirent_t *fat32_stat(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory");

  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  // static fat32_dirent_t *get_dirents(fat32_fs_t *fs, uint32_t cluster_start, uint32_t *dir_n)
  uint32_t dir_n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &dir_n);

  // TODO: Iterate through the directory's entries and find a dirent with the
  // provided name.  Return NULL if no such dirent exists.  You can use
  // `find_dirent_with_name` if you've implemented it.
  int dirent_index = find_dirent_with_name(dirents, dir_n, filename);
  if (dirent_index == -1) {
    return NULL;
  }
  
  // TODO: allocate enough space for the dirent, then convert
  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  // static pi_dirent_t dirent_convert(fat32_dirent_t *d) 
  *dirent = dirent_convert(&dirents[dirent_index]);
  return dirent;
}

// note: fat32 efilesystem is on sd card in pi

pi_file_t *fat32_read(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  // This should be pretty similar to readdir, but simpler.
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // TODO: read the dirents of the provided directory and look for one matching the provided name
  pi_dirent_t *file_match = fat32_stat(fs, directory, filename);

  // TODO: figure out the length of the cluster chain
  uint32_t num_clusters = get_cluster_chain_length(fs, file_match->cluster_id);
  if (num_clusters == 0) {
    // no data to display
  }

  // TODO: allocate a buffer large enough to hold the whole file
  void *file_buffer = NULL;

  // TODO: read in the whole file (if it's not empty)
  if (file_match->nbytes > 0) {
    // read in file
    file_buffer = kmalloc(num_clusters * fs->sectors_per_cluster * boot_sector.bytes_per_sec);
    read_cluster_chain(fs, file_match->cluster_id, file_buffer);

  }

  // TODO: fill the pi_file_t
  pi_file_t *file = kmalloc(sizeof(pi_file_t));
  *file = (pi_file_t) {
    .data = file_buffer,
    .n_data = file_match->nbytes,
    .n_alloc = num_clusters * fs->sectors_per_cluster * boot_sector.bytes_per_sec,
  };
  return file;
}

/******************************************************************************
 * Everything below here is for writing to the SD card (Part 7/Extension).  If
 * you're working on read-only code, you don't need any of this.
 ******************************************************************************/

static uint32_t find_free_cluster(fat32_fs_t *fs, uint32_t start_cluster) {
  // TODO: loop through the entries in the FAT until you find a free one
  // (fat32_fat_entry_type == FREE_CLUSTER).  Start from cluster 3.  Panic if
  // there are none left.
  if (start_cluster < 3) start_cluster = 3;

  for (int i = start_cluster; i < fs->n_entries; i++) {
    if (fat32_fat_entry_type(fs->fat[i]) == FREE_CLUSTER) {
      return i;
    }
  }

  if (trace_p) trace("failed to find free cluster from %d\n", start_cluster);
  panic("No more clusters on the disk!\n");
}

static void write_fat_to_disk(fat32_fs_t *fs) {
  // TODO: Write the FAT to disk.  In theory we should update every copy of the
  // FAT, but the first one is probably good enough.  A good OS would warn you
  // if the FATs are out of sync, but most OSes just read the first one without
  // complaining.
  if (trace_p) trace("syncing FAT\n");
  //unimplemented();
  pi_sd_write(fs->fat, fs->fat_begin_lba, boot_sector.nsec_per_fat);

}

static uint32_t min(uint32_t a, uint32_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

static void write_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data, uint32_t nbytes) {
  if (nbytes == 0) {
    if (trace_p) trace("write_cluster_chain: nbytes is 0, nothing to write\n");
    return;
  }
  // ASSUME DATA IS A MULTIPLE OF CLUSTER SIZE
  // SUZE: edit to use beautiful min function
  if (trace_p) trace("writing %d bytes to cluster %d\n", nbytes, start_cluster);
  

  uint32_t cluster = start_cluster;
  uint32_t bytes_written = 0;
  while (bytes_written < nbytes) {
    uint32_t bytes_to_write = nbytes - bytes_written;
    uint32_t bytes_per_cluster = boot_sector.bytes_per_sec * fs->sectors_per_cluster;
    bytes_to_write = min(bytes_to_write, bytes_per_cluster);

    uint32_t lba = cluster_to_lba(fs, cluster);
    pi_sd_write(data + bytes_written, lba, fs->sectors_per_cluster);
    bytes_written += bytes_to_write;
    if (fat32_fat_entry_type(cluster) == LAST_CLUSTER) {
      uint32_t new_cluster = find_free_cluster(fs, cluster);
      fs->fat[cluster] = new_cluster;
      fs->fat[new_cluster] = LAST_CLUSTER;
      write_fat_to_disk(fs);
      cluster = new_cluster;
    } else {
      cluster = fs->fat[cluster];
    }
  }

  uint32_t next_cluster = cluster;
  while (fat32_fat_entry_type(next_cluster) != LAST_CLUSTER && fat32_fat_entry_type(next_cluster) != FREE_CLUSTER) {
    uint32_t temp = next_cluster;
    next_cluster = fs->fat[next_cluster];
    fs->fat[temp] = FREE_CLUSTER;
  }
  fs->fat[cluster] = LAST_CLUSTER;
  write_fat_to_disk(fs);
}



int fat32_rename(fat32_fs_t *fs, pi_dirent_t *directory, char *oldname, char *newname) {
  // TODO: Get the dirents `directory` off the disk, and iterate through them
  // looking for the file.  When you find it, rename it and write it back to
  // the disk (validate the name first).  Return 0 in case of an error, or 1
  // on success.
  // Consider:
  //  - what do you do when there's already a file with the new name?
  demand(init_p, "fat32 not initialized!");
  if (!fat32_is_valid_name(newname)) return 0;
  if (trace_p) trace("renaming %s to %s\n", oldname, newname);

  // TODO: get the dirents and find the right one
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_to_rename = find_dirent_with_name(dirents, n_dirents, oldname);
  if (dirent_to_rename == -1) {
    panic("cannot find old file");
    return 0;
  }
  if (find_dirent_with_name(dirents, n_dirents, newname) != -1) {
    panic("file with new name already exists\n");
    return 0;
  }

  // TODO: update the dirent's name
  fat32_dirent_set_name(&dirents[dirent_to_rename], newname);

  // TODO: write out the directory, using the existing cluster chain (or
  // appending to the end); implementing `write_cluster_chain` will help
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, n_dirents * sizeof(fat32_dirent_t));
  
  return 1;

}

// Create a new directory entry for an empty file (or directory).
pi_dirent_t *fat32_create(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, int is_dir) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("creating %s\n", filename);
  if (!fat32_is_valid_name(filename)) {
    if (trace_p) trace("not a valid filename %s\n", filename);
    return NULL;
  }

  // TODO: read the dirents and make sure there isn't already a file with the same name
  uint32_t dir_n;
  fat32_dirent_t *fat_dirents = get_dirents(fs, directory->cluster_id, &dir_n);
  int dirent_index = find_dirent_with_name(fat_dirents, dir_n, filename);
  if (dirent_index != -1) {
    panic("a file with this name already exists");
  }

  // TODO: look for a free directory entry and use it to store the data for the
  // new file.  If there aren't any free directory entries, either panic or
  // (better) handle it appropriately by extending the directory to a new
  // cluster.
  // When you find one, update it to match the name and attributes
  // specified; set the size and cluster to 0.
  //unimplemented();
  int dirent_want_idx = -1;
  for (int i = 0; i < dir_n; i++) {
    //fat32_dirent_t fat32_dirent = fat_dirents[i];
    // check if the entry is free
    if (fat32_dirent_free(&fat_dirents[i])) {
      dirent_want_idx = i;
      break;
    }
  }
  if (dirent_want_idx == -1) {
    panic("no free entries in directory"); // TODO: handle this better (see description)
  }

  fat32_dirent_t *fat32_entry = &fat_dirents[dirent_want_idx];
  fat32_dirent_set_name(fat32_entry, filename);
  if (is_dir) {
    fat32_entry->attr = FAT32_DIR;
  }
  else {
    fat32_entry->attr = FAT32_ARCHIVE;
  }
  fat32_entry->file_nbytes = 0;
  fat32_entry->hi_start = 0; // setting cluster to 0; make sure we handle reading this correctly!
  fat32_entry->lo_start = 0; 

  // TODO: write out the updated directory to the disk
  write_fat_to_disk(fs);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)fat_dirents, dir_n * sizeof(fat32_dirent_t));
  
  // dirent_want_idx / [entries in a cluster] = cluster number
  // dirent_want_idx % [entries in a sector] = sector number


  // TODO: convert the dirent to a `pi_dirent_t` and return a (kmalloc'ed) pointer
  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  *dirent = dirent_convert(fat32_entry);
  return dirent;
}

// Delete a file, including its directory entry.
int fat32_delete(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  // TODO: edit suze
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("deleting %s\n", filename);
  if (!fat32_is_valid_name(filename)) return 0;
  // TODO: look for a matching directory entry, and set the first byte of the
  // name to 0xE5 to mark it as free
  // unimplemented();
  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_want = find_dirent_with_name(dirents, n_dirents, filename);
  if (dirent_want == -1) {
    trace("no matching file to delete\n");
    return 1;
  }
  trace("dirent_want to delete: %d\n", dirent_want);
  dirents[dirent_want].filename[0] = 0xE5;

  // pi_dirent_t *dirent = fat32_stat(fs, directory, filename);
  // if (!dirent) {
  //   trace("no matching directory entry\n");
  //   return 0;
  // }
  // uint32_t cluster = dirent->cluster_id;
  uint32_t cluster = dirents[dirent_want].lo_start | (dirents[dirent_want].hi_start << 16);


  // TODO: free the clusters referenced by this dirent
  // unimplemented();
  // uint32_t cluster = dirents[dirent_want].lo_start | (dirents[dirent_want].hi_start << 16);
  //retrieved->name[0] = 0xE5;
  //uint32_t cluster = retrieved->cluster_id;
  
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, n_dirents * sizeof(fat32_dirent_t));
  while (fat32_fat_entry_type(cluster) != FREE_CLUSTER && fat32_fat_entry_type(cluster) != LAST_CLUSTER) {
    if (trace_p) trace("freeing cluster %d\n", cluster);
    uint32_t temp = cluster;
    cluster = fs->fat[cluster];
    if (trace_p) trace("next freeing cluster %d\n", cluster);
    fs->fat[temp] = FREE_CLUSTER;
  }
  fs->fat[cluster] = FREE_CLUSTER;

  // TODO: write out the updated directory to the disk
  // unimplemented();
  if (trace_p) trace("writing out the updated directory\n");
  write_fat_to_disk(fs);
  
  return 1;
}

int fat32_truncate(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, unsigned length) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("truncating %s\n", filename);

  uint32_t n_dirents;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &n_dirents);
  int dirent_want_idx = find_dirent_with_name(dirents, n_dirents, filename);
  if (dirent_want_idx == -1) {
    trace("no matching directory to truncate found\n");
    return 0;
  }
  fat32_dirent_t *dirent_to_truncate = &dirents[dirent_want_idx];
  dirent_to_truncate->file_nbytes = length;
  pi_file_t *cur_file = fat32_read(fs, directory, filename);
  // should be writing what is currently in the file
  write_cluster_chain(fs, dirent_to_truncate->lo_start | (dirent_to_truncate->hi_start << 16), cur_file->data, length);


  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, n_dirents * sizeof(fat32_dirent_t));
  write_fat_to_disk(fs);
  return 1;
}

int fat32_write(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, pi_file_t *file) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  uint32_t dir_n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &dir_n);
  int dirent_index = find_dirent_with_name(dirents, dir_n, filename);
  if (dirent_index == -1) {
    trace("no matching directory entry\n");
    return 0; // exit with error?
  }
  // update the directory entry with the new size

  fat32_dirent_t *dirent_to_write = &dirents[dirent_index];
  dirent_to_write->file_nbytes = file->n_data;
  if (dirent_to_write->lo_start == 0 && dirent_to_write->hi_start == 0) {
    trace("empty file, allocating new cluster\n");
    uint32_t new_cluster = find_free_cluster(fs, 0);
    dirent_to_write->lo_start = new_cluster & 0xFFFF;
    dirent_to_write->hi_start = (new_cluster >> 16) & 0xFFFF;
    fs->fat[new_cluster] = LAST_CLUSTER;
  }
  // write out file data
  write_cluster_chain(fs, dirent_to_write->lo_start | (dirent_to_write->hi_start << 16), file->data, file->n_data);

  // write out directory entries
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, dir_n * sizeof(fat32_dirent_t));

  // write out fat
  write_fat_to_disk(fs);
  return 1;
}

int fat32_flush(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // no-op
  return 0;
}
