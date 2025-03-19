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
  uint32_t cur_cluster = start_cluster;

  for (int i = start_cluster; i < fs->n_entries; i++) {
    if (fat32_fat_entry_type(cur_cluster) == FREE_CLUSTER) {
      return cur_cluster;
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

// Given the starting cluster index, write the data in `data` over the
// pre-existing chain, adding new clusters to the end if necessary.
static void write_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data, uint32_t nbytes) { // uint8_t = 8 bits
  // ASSUME DATA IS A MULTIPLE OF CLUSTER SIZE

  // Walk the cluster chain in the FAT, writing the in-memory data to the
  // referenced clusters.  If the data is longer than the cluster chain, find
  // new free clusters and add them to the end of the list.
  // Things to consider:
  //  - what if the data is shorter than the cluster chain?
  //  - what if the data is longer than the cluster chain?
  //  - the last cluster needs to be marked LAST_CLUSTER
  //  - when do we want to write the updated FAT to the disk to prevent
  //  corruption?
  //  - what do we do when nbytes is 0?
  //  - what about when we don't have a valid cluster to start with? !!!
  //
  //  This is the main "write" function we'll be using; the other functions
  //  will delegate their writing operations to this.

  // TODO: As long as we have bytes left to write and clusters to write them
  // to, walk the cluster chain writing them out.
  //unimplemented();
  uint32_t nbytes_used = 0;
  uint32_t cur_cluster = start_cluster;
  if (fat32_fat_entry_type(cur_cluster) == LAST_CLUSTER) { // ASK JOE
    panic("we cannot write data to this location (input start_cluster == LAST_CLUSTER)");
  }


  uint32_t clusters_available = get_cluster_chain_length(fs, start_cluster);
  uint32_t clusters_used = 0;
  uint32_t last_real_cluster = start_cluster; // used so that it won't be set to LAST_CLUSTER

  while ((fat32_fat_entry_type(cur_cluster) != LAST_CLUSTER) && (nbytes_used < nbytes)) {
    // populate this cluster with bytes
    //uint32_t nsec_to_write = min(bytes_per_cluster, nbytes - nbytes_used) / boot_sector.bytes_per_sec;
    uint32_t lba_to_use = cluster_to_lba(fs, cur_cluster);
    pi_sd_write(&data[nbytes_used], lba_to_use, boot_sector.sec_per_cluster); 
    nbytes_used += boot_sector.sec_per_cluster * boot_sector.bytes_per_sec;
    last_real_cluster = cur_cluster;
    cur_cluster = fs->fat[cur_cluster];
    clusters_used++;
  }

  // TODO: If we run out of clusters to write to, find free clusters using the
  // FAT and continue writing the bytes out.  Update the FAT to reflect the new
  // cluster.
  //unimplemented();
  cur_cluster = last_real_cluster; // thisn takes us one step back sice cur_cluster is now set to LAST_CLUSTER
  while (nbytes_used < nbytes) {
    // we ran out of clusters
    uint32_t next_cluster = find_free_cluster(fs, cur_cluster);
    fs->fat[cur_cluster] = next_cluster;
    
    uint32_t lba_to_use = cluster_to_lba(fs, cur_cluster);
    pi_sd_write(&data[nbytes_used], lba_to_use, boot_sector.sec_per_cluster); 
    nbytes_used += boot_sector.sec_per_cluster * boot_sector.bytes_per_sec;

    last_real_cluster = cur_cluster;
    cur_cluster = next_cluster;
    clusters_used++;
  }

  
  // TODO: If we run out of bytes to write before using all the clusters, mark
  // the final cluster as "LAST_CLUSTER" in the FAT, then free all the clusters
  // later in the chain.
  cur_cluster = last_real_cluster;
  //unimplemented();
  //int32_t clusters_to_free = clusters_available - clusters_used; // cannot be uint32_t bc could be negative
  while (clusters_used < clusters_available) {
    cur_cluster = fs->fat[cur_cluster];
    fs->fat[cur_cluster] = FREE_CLUSTER;
    clusters_used++;
  }

  // TODO: Ensure that the last cluster in the chain is marked "LAST_CLUSTER".
  // The one exception to this is if we're writing 0 bytes in total, in which
  // case we don't want to use any clusters at all.
  //unimplemented();
  if (nbytes != 0) {
    fs->fat[last_real_cluster] = LAST_CLUSTER;
  }
}

int fat32_rename(fat32_fs_t *fs, pi_dirent_t *directory, char *oldname, char *newname) {
  // TODO: Get the dirents `directory` off the disk, and iterate through them
  // looking for the file.  When you find it, rename it and write it back to
  // the disk (validate the name first).  Return 0 in case of an error, or 1
  // on success.
  // Consider:
  //  - what do you do when there's already a file with the new name?
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("renaming %s to %s\n", oldname, newname);
  if (!fat32_is_valid_name(newname)) return 0;

  // TODO: get the dirents and find the right one
  unimplemented();

  // TODO: update the dirent's name
  unimplemented();

  // TODO: write out the directory, using the existing cluster chain (or
  // appending to the end); implementing `write_cluster_chain` will help
  unimplemented();
  return 1;

}

// Create a new directory entry for an empty file (or directory).
pi_dirent_t *fat32_create(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, int is_dir) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("creating %s\n", filename);
  if (!fat32_is_valid_name(filename)) return NULL;

  // TODO: read the dirents and make sure there isn't already a file with the same name
  uint32_t dir_n;
  fat32_dirent_t *fat_dirents = get_dirents(fs, directory->cluster_id, &dir_n);
  int dirent_index = find_dirent_with_name(fat_dirents, dir_n, filename);
  if (dirent_index != -1) {
    panic("a file with this name already exists");
  }
  //unimplemented();

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
  // TODO SUZE: write cluster chain to disk DO THE SAME FOR DELETE!!!
  //pi_sd_write(fs->fat, fs->fat_begin_lba, boot_sector.nsec_per_fat); // one entry is 32 bytes, cluster is way more
  //pi_sd_write(???); use write_cluster_chain?? 


  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)fat_dirents, dir_n * sizeof(fat32_dirent_t));// ASK JOE
  // dirent_want_idx / [entries in a cluster] = cluster number
  // dirent_want_idx % [entries in a sector] = sector number
  //unimplemented();


  // TODO: convert the dirent to a `pi_dirent_t` and return a (kmalloc'ed) pointer
  //unimplemented();
  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  *dirent = dirent_convert(fat32_entry);
  return dirent;
}

// Delete a file, including its directory entry.
int fat32_delete(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("deleting %s\n", filename);
  if (!fat32_is_valid_name(filename)) return 0;
  // TODO: look for a matching directory entry, and set the first byte of the
  // name to 0xE5 to mark it as free
  pi_dirent_t *retrieved_entry = fat32_stat(fs, directory, filename); // should not be using this; just get all entries
  retrieved_entry->name[0] = 0xE5; // mark as free
  unimplemented();

  // TODO: free the clusters referenced by this dirent
  uint32_t cur_cluster = retrieved_entry->cluster_id;

  while ((fat32_fat_entry_type(cur_cluster) != LAST_CLUSTER) && (fat32_fat_entry_type(cur_cluster) != FREE_CLUSTER)) { 
      uint32_t temp_cluster = cur_cluster;
      cur_cluster = fs->fat[cur_cluster];
      fs->fat[temp_cluster] = FREE_CLUSTER;
  }
  fs->fat[cur_cluster] = FREE_CLUSTER;
  //unimplemented();

  // TODO: write out the updated directory to the disk ASK JOE
  //unimplemented();
  write_fat_to_disk(fs);
  //write_cluster_chain(fs, directory->cluster_id, (uint8_t *)fat_dirents, dir_n * sizeof(fat32_dirent_t));
  return 1;
}

int fat32_truncate(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, unsigned length) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("truncating %s\n", filename);

  // TODO: edit the directory entry of the file to list its length as `length` bytes,
  // then modify the cluster chain to either free unused clusters or add new
  // clusters.
  //
  // Consider: what if the file we're truncating has length 0? what if we're
  // truncating to length 0?
  unimplemented();

  // TODO: write out the directory entry
  unimplemented();
  return 0;
}

int fat32_write(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, pi_file_t *file) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // TODO: Surprisingly, this one should be rather straightforward now.
  // - load the directory
  // - exit with an error (0) if there's no matching directory entry
  // - update the directory entry with the new size
  // - write out the file as clusters & update the FAT
  // - write out the directory entry
  // Special case: the file is empty to start with, so we need to update the
  // start cluster in the dirent

  uint32_t dir_n;
  fat32_dirent_t *dirents = get_dirents(fs, directory->cluster_id, &dir_n);
  int dirent_index = find_dirent_with_name(dirents, dir_n, filename);
  if (dirent_index == -1) {
    return 0; // exit with error?
  }
  // update the directory entry with the new size




  unimplemented();
}

int fat32_flush(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // no-op
  return 0;
}