#include "rpi.h"
#include "fat32.h"
#include "fat32-helpers.h"
#include "pi-sd.h"

// Print extra tracing info when this is enabled.  You can and should add your
// own.
static int trace_p = 1; 
static int init_p = 0;

fat32_boot_sec_t boot_sector;


// DONE
fat32_fs_t fat32_mk(mbr_partition_ent_t *partition) {
  demand(!init_p, "the fat32 module is already in use\n");
  // TODO: Read the boot sector (of the partition) off the SD card.
  fat32_boot_sec_t *boot_sector_p = pi_sec_read(partition->lba_start, 1);
  boot_sector = *boot_sector_p;

  // TODO: Verify the boot sector (also called the volume id, `fat32_volume_id_check`)
  fat32_volume_id_check(boot_sector_p); ///fat32_boot_sec_t *b

  // TODO: Read the FS info sector (the sector immediately following the boot
  // sector) and check it (`fat32_fsinfo_check`, `fat32_fsinfo_print`)
  assert(boot_sector.info_sec_num == 1);
  // void fat32_fsinfo_check(struct fsinfo *info);
  struct fsinfo *fsinfo = (struct fsinfo *)(pi_sec_read(partition->lba_start + 1, 1)); // read sector after boot ==> fsinfo
  fat32_fsinfo_check(fsinfo);
  fat32_fsinfo_print("fsinfo printed: ", fsinfo);

  // END OF PART 2
  // The rest of this is for Part 3:

  // TODO: calculate the fat32_fs_t metadata, which we'll need to return.
  unsigned lba_start = -1; // from the partition
  unsigned fat_begin_lba = -1; // the start LBA + the number of reserved sectors
  unsigned cluster_begin_lba = -1; // the beginning of the FAT, plus the combined length of all the FATs
  unsigned sec_per_cluster = -1; // from the boot sector
  unsigned root_first_cluster = -1; // from the boot sector
  unsigned n_entries = -1; // from the boot sector

  // aditi wrote this!!
  lba_start = partition->lba_start;
  fat_begin_lba = lba_start + boot_sector_p->reserved_area_nsec; // double check this is the num of reserved sectors
  cluster_begin_lba = lba_start + boot_sector_p->reserved_area_nsec + (boot_sector_p->nfats * boot_sector_p->nsec_per_fat);
  sec_per_cluster = boot_sector_p->sec_per_cluster;
  root_first_cluster = boot_sector_p->first_cluster;
  n_entries = boot_sector_p->nsec_per_fat * boot_sector_p->bytes_per_sec / 4;  // div by 4 bc 32 bit = 4 bytes per entry

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
  fat = (uint32_t *)(pi_sec_read(fat_begin_lba, boot_sector_p->nsec_per_fat));

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
// TODO
static uint32_t cluster_to_lba(fat32_fs_t *f, uint32_t cluster_num) {
  assert(cluster_num >= 2);
  // TODO: calculate LBA from cluster number, cluster_begin_lba, and
  // sectors_per_cluster
  unsigned lba = (uint32_t)(f->cluster_begin_lba + (cluster_num - 2) * f->sectors_per_cluster);  // from prelab

  // unsigned lba;
  if (trace_p) trace("cluster %d to lba: %d\n", cluster_num, lba);
  return lba;
}

// DONE
pi_dirent_t fat32_get_root(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // TODO: return the information corresponding to the root directory (just
  // cluster_id, in this case)

  return (pi_dirent_t) {
    .name = "",
      .raw_name = "",
      .cluster_id = fs->root_dir_first_cluster, // ADITI EDIT
      .is_dir_p = 1,
      .nbytes = 0,//fs->n_entries * 32,
  };
}

// Given the starting cluster index, get the length of the chain.  Helper
// function.
// DONE
static uint32_t get_cluster_chain_length(fat32_fs_t *fs, uint32_t start_cluster) {
  // TODO: Walk the cluster chain in the FAT until you see a cluster where
  // `fat32_fat_entry_type(cluster) == LAST_CLUSTER`.  Count the number of
  // clusters.

  if (start_cluster == 0) return 0;

  uint32_t cur_cluster = start_cluster;
  uint32_t num_clusters = 0; // TODO potential issue - could be 1

  // check if it's the last cluster
  while (fat32_fat_entry_type(cur_cluster) != LAST_CLUSTER) { 
    // increment and go to the next cluster
    cur_cluster = fs->fat[cur_cluster];
    num_clusters++;
  }

  return num_clusters;
}

// Given the starting cluster index, read a cluster chain into a contiguous
// buffer.  Assume the provided buffer is large enough for the whole chain.
// Helper function.
// DONE
static void read_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data) {
  // TODO: Walk the cluster chain in the FAT until you see a cluster where
  // fat32_fat_entry_type(cluster) == LAST_CLUSTER.  For each cluster, copy it
  // to the buffer (`data`).  Be sure to offset your data pointer by the
  // appropriate amount each time.

  data[0] = '\0';
  if (start_cluster == 0) return;

  uint32_t cur_cluster = start_cluster;
  uint8_t *next_data = data;

  // check if it's the last cluster
  while (fat32_fat_entry_type(cur_cluster) != LAST_CLUSTER) {
    uint32_t cluster_lba = (uint32_t)(cluster_to_lba(fs, cur_cluster));  // from prelab

    // read 1 sector from the cluster into data
    // should read from sd card
    void *read_data = pi_sec_read(cluster_lba, fs->sectors_per_cluster);
    memcpy(next_data, read_data, fs->sectors_per_cluster * boot_sector.bytes_per_sec);
    next_data += fs->sectors_per_cluster * boot_sector.bytes_per_sec; // go to the next open spot in next data pointer
    
    // go to the next cluster
    cur_cluster = fs->fat[cur_cluster];
  }

}

// Converts a fat32 internal dirent into a generic one suitable for use outside
// this driver.
// DONE
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
// DONE
static fat32_dirent_t *get_dirents(fat32_fs_t *fs, uint32_t cluster_start, uint32_t *dir_n) {
  // TODO: figure out the length of the cluster chain (see
  // `get_cluster_chain_length`)
  uint32_t cl_ch_len = get_cluster_chain_length(fs, cluster_start);
  
  // TODO: allocate a buffer large enough to hold the whole directory
  // WHAT SHOULD THIS BE??
  uint32_t amnt_alloc = cl_ch_len * boot_sector.sec_per_cluster * boot_sector.bytes_per_sec;
  uint8_t *data = kmalloc(amnt_alloc); 

  // TODO: read in the whole directory (see `read_cluster_chain`)
  read_cluster_chain(fs, cluster_start, data);

  *dir_n = amnt_alloc / sizeof(fat32_dirent_t);
  return (fat32_dirent_t *)data;
  // return (fat32_dirent_t *)NULL;
}


// DONE
pi_directory_t fat32_readdir(fat32_fs_t *fs, pi_dirent_t *dirent) {
  demand(init_p, "fat32 not initialized!");
  demand(dirent->is_dir_p, "tried to readdir a file!");
  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  uint32_t n_dirents;
  fat32_dirent_t *fat_dirents = get_dirents(fs, dirent->cluster_id, &n_dirents);

  // TODO: allocate space to store the pi_dirent_t return values
  // 16 dir entries in a sector. can be multiple sectors
  pi_dirent_t *pi_dirents = kmalloc(n_dirents * sizeof(pi_dirent_t)); // TODO COME BACK!!

  // TODO: iterate over the directory and create pi_dirent_ts for every valid
  // file.  Don't include empty dirents, LFNs, or Volume IDs.  You can use
  // `dirent_convert`.
  int pi_j = 0;
  for (int i = 0; i < n_dirents; i++) {
    if (fat32_dirent_free(&fat_dirents[i])) continue; // free space
    if (fat32_dirent_is_lfn(&fat_dirents[i])) continue; // LFN version of name
    if (fat_dirents[i].attr & FAT32_VOLUME_LABEL) continue; // volume label

    pi_dirents[pi_j] = dirent_convert(&fat_dirents[i]);
    pi_j++;
  }

  // TODO: create a pi_directory_t using the dirents and the number of valid
  // dirents we found
  return (pi_directory_t) {
    .dirents = pi_dirents, // aditi edit
    .ndirents = pi_j, // these are the valid pi_dirents I copied into pi_dirents
  };
}

// DONE
static int find_dirent_with_name(fat32_dirent_t *dirents, int n, char *filename) {
  // TODO: iterate through the dirents, looking for a file which matches the
  // name; use `fat32_dirent_name` to convert the internal name format to a
  // normal string.
  for (int i = 0; i < n; i++) {
    // check if the first 11 chars are the same
    char name[15];
    fat32_dirent_name(&dirents[i], name); // convert the file name to fat format
    if (strcmp(name, filename) == 0) return i;
  }

  return -1;
}

// DONE
pi_dirent_t *fat32_stat(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory");

  // TODO: use `get_dirents` to read the raw dirent structures from the disk
  uint32_t dir_n = 0;
  fat32_dirent_t *fat_dirents = get_dirents(fs, directory->cluster_id, &dir_n);

  // TODO: Iterate through the directory's entries and find a dirent with the
  // provided name.  Return NULL if no such dirent exists.  You can use
  // `find_dirent_with_name` if you've implemented it.
  int found_name_index = find_dirent_with_name(fat_dirents, dir_n, filename);
  if (found_name_index == -1) return NULL;

  // found_name
  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  *dirent = dirent_convert(&fat_dirents[found_name_index]);

  // TODO: allocate enough space for the dirent, then convert
  // (`dirent_convert`) the fat32 dirent into a Pi dirent.
  // pi_dirent_t *dirent = NULL;
  return dirent;
}

// DONE
pi_file_t *fat32_read(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  // This should be pretty similar to readdir, but simpler.
  demand(init_p, "fat32 not initialized!");
  demand(directory->is_dir_p, "tried to use a file as a directory!");

  // TODO: read the dirents of the provided directory and look for one matching the provided name
  pi_dirent_t* pi_dirent = fat32_stat(fs, directory, filename);
  if (pi_dirent == NULL) {printk ("ERROR!"); return NULL;}

  // TODO: figure out the length of the cluster chain
  uint32_t cl_ch_len = get_cluster_chain_length(fs, pi_dirent->cluster_id);

  // TODO: allocate a buffer large enough to hold the whole file
  uint32_t num_bytes = pi_dirent->nbytes;

  uint8_t *buf = kmalloc(cl_ch_len * 512 * fs->sectors_per_cluster); // * num_bytes * 512 / fs->sectors_per_cluster);

  // TODO: read in the whole file (if it's not empty)
  read_cluster_chain(fs, pi_dirent->cluster_id, buf);

  // TODO: fill the pi_file_t
  pi_file_t *file = kmalloc(sizeof(pi_file_t));
  *file = (pi_file_t) {
    .data = buf,  // aditi edit!
    .n_data = num_bytes,
    .n_alloc = cl_ch_len * 512 * fs->sectors_per_cluster,
  };
  return file;
}


/******************************************************************************
 * Everything below here is for writing to the SD card (Part 7/Extension).  If
 * you're working on read-only code, you don't need any of this.
 ******************************************************************************/

// DONE
static uint32_t find_free_cluster(fat32_fs_t *fs, uint32_t start_cluster) {
  // TODO: loop through the entries in the FAT until you find a free one
  // (fat32_fat_entry_type == FREE_CLUSTER).  Start from cluster 3.  Panic if
  // there are none left.
  if (start_cluster < 3) start_cluster = 3;

  // uint32_t cur_cluster = start_cluster;

  uint32_t num_clusters = fs->n_entries;
  for (int i = start_cluster; i < num_clusters; i++) {
    if (fat32_fat_entry_type(fs->fat[i]) == FREE_CLUSTER) return i;
    // cur_cluster += 1;
  }

  if (trace_p) trace("failed to find free cluster from %d\n", start_cluster);
  panic("No more clusters on the disk!\n");
}

// DONE
static void write_fat_to_disk(fat32_fs_t *fs) {
  // TODO: Write the FAT to disk.  In theory we should update every copy of the
  // FAT, but the first one is probably good enough.  A good OS would warn you
  // if the FATs are out of sync, but most OSes just read the first one without
  // complaining

  if (trace_p) trace("syncing FAT\n");
  pi_sd_write(fs->fat, fs->fat_begin_lba, boot_sector.nsec_per_fat);
}


// credit: Suze
static uint32_t min(uint32_t a, uint32_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

// Given the starting cluster index, write the data in `data` over the
// pre-existing chain, adding new clusters to the end if necessary.
// STARTED
static void write_cluster_chain(fat32_fs_t *fs, uint32_t start_cluster, uint8_t *data, uint32_t nbytes) {
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
  //  - what about when we don't have a valid cluster to start with?
  //
  //  This is the main "write" function we'll be using; the other functions
  //  will delegate their writing operations to this.


  // CREDIT: Suze

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




// DONE
int fat32_rename(fat32_fs_t *fs, pi_dirent_t *directory, char *oldname, char *newname) {
  // TODO: Get the dirents `directory` off the disk, and iterate through them
  // looking for the file.  When you find it, rename it and write it back to
  // the disk (validate the name first).  Return 0 in case of an error, or 1
  // on success.
  // Consider:
  //  - what do you do when there's already a file with the new name?

  // Credit: Suze

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

// DONE
// Create a new directory entry for an empty file (or directory).
pi_dirent_t *fat32_create(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, int is_dir) {
  demand(init_p, "fat32 not initialized!");
  if (trace_p) trace("creating %s\n", filename);
  if (!fat32_is_valid_name(filename)) return NULL;

  // TODO: read the dirents and make sure there isn't already a file with the
  // same name

  // check that the directory is actually a directory 
  if (!directory->is_dir_p) {printk("directory is not a dir NOT FOUND!"); return NULL;}

  uint32_t dir_n = 0;
  fat32_dirent_t *fat_dirents = get_dirents(fs, directory->cluster_id, &dir_n);
  int dirent_with_name = find_dirent_with_name(fat_dirents, dir_n, filename);
  if (dirent_with_name != -1) {printk("dirent_with_name already exists!"); return NULL;}

  // TODO: look for a free directory entry and use it to store the data for the
  // new file.  If there aren't any free directory entries, either panic or
  // (better) handle it appropriately by extending the directory to a new
  // cluster.
  // When you find one, update it to match the name and attributes
  // specified; set the size and cluster to 0.
  int dir_found = -1;
  for (int i = 0; i < dir_n; i++) {
    fat32_dirent_t *fdir = &fat_dirents[i];
    if (fat32_dirent_free(fdir)) {
      dir_found = i;
      // set name
      fat32_dirent_set_name(fdir, filename); // dirent entry, then file name
      // set attr
      fdir->attr = is_dir ? FAT32_DIR : FAT32_ARCHIVE;
      // set size
      fdir->file_nbytes = 0; // set to 0 all the time
      // set cluster - sus ?? TODO CHECK is this how we handle the cluster correctly??
      fdir->hi_start = 0;
      fdir->lo_start = 0;
      break;
    }
  }
  if (dir_found == -1) {panic("dir_found NOT FOUND!");}

  // TODO: write out the updated directory to the disk
  // joe comment: [do the math] figure out sector num i of entry, and divide by number of sectors in cluster. then jst write back that specific cluster
  // figure out which sector and just write that one back
  write_fat_to_disk(fs);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)fat_dirents, dir_n * sizeof(fat32_dirent_t));

  // TODO: convert the dirent to a `pi_dirent_t` and return a (kmalloc'ed) pointer
  // dir_found / [entries in a cluster] = cluster num
  // dir_found % [entries in a sector] = sector num
  pi_dirent_t *dirent = kmalloc(sizeof(pi_dirent_t));
  *dirent = dirent_convert(&fat_dirents[dir_found]);
  return dirent;
}



// Delete a file, including its directory entry.
int fat32_delete(fat32_fs_t *fs, pi_dirent_t *directory, char *filename) {
  
  // CREDIT: Suze

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

  uint32_t cluster = dirents[dirent_want].lo_start | (dirents[dirent_want].hi_start << 16);

  // TODO: free the clusters referenced by this dirent
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
  if (trace_p) trace("writing out the updated directory\n");
  write_fat_to_disk(fs);
  
  return 1;
}

int fat32_truncate(fat32_fs_t *fs, pi_dirent_t *directory, char *filename, unsigned length) {
  // Credit: Suze
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

  // credit: Suze
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
