// NOTE: The .o of fat32.c has a #include to include the code in this file at the end.

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

  if (nbytes == 0) {
    if (trace_p) trace("write_cluster_chain: nbytes is 0, nothing to write\n");
    return;
  }

  // // aditi edit for files->dir ??
  // if (start_cluster < 2) {
  //   panic("write_cluster_chain: Invalid start cluster %d", start_cluster);
  //   return;
  // }

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
  if (!fat32_is_valid_name(filename)) {printk("invalid fat32 file name!"); return NULL;} // NOTE: can't add _ in file name

  // TODO: read the dirents and make sure there isn't already a file with the same name

  // check that the directory is actually a directory 
  if (!directory->is_dir_p) {printk("directory is not a dir NOT FOUND!\n"); return NULL;}

  uint32_t dir_n = 0;
  fat32_dirent_t *fat_dirents = get_dirents(fs, directory->cluster_id, &dir_n);
  int dirent_with_name = find_dirent_with_name(fat_dirents, dir_n, filename);
  if (dirent_with_name != -1) {printk("dirent_with_name already exists!\n"); return NULL;}

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
      

      
      // aditi edit: new code!
      // for directories, need to alloc a cluster
      if (is_dir) { 
        // Allocate a new cluster for the directory contents
        uint32_t new_cluster = find_free_cluster(fs, 0);
        if (new_cluster == 0) {
          printk("Failed to allocate cluster for new directory\n");
          return NULL;
        }
        
        // Mark the cluster as the end of a chain
        fs->fat[new_cluster] = LAST_CLUSTER;
        
        // Set the starting cluster in the directory entry
        fdir->hi_start = (new_cluster >> 16) & 0xFFFF;
        fdir->lo_start = new_cluster & 0xFFFF;
        
        // Initialize the directory cluster (clearing it)
        uint32_t bytes_per_cluster = fs->sectors_per_cluster * boot_sector.bytes_per_sec;
        uint8_t *empty_cluster = kmalloc(bytes_per_cluster);
        memset(empty_cluster, 0, bytes_per_cluster);
        
        // Write the empty cluster
        // uint32_t bytes_per_cluster = fs->sectors_per_cluster * boot_sector.bytes_per_sec;
        write_cluster_chain(fs, new_cluster, empty_cluster, bytes_per_cluster);
        // kfree(empty_cluster);
      } 
      
      // For files, start with no clusters
      else {
        
        fdir->hi_start = 0;
        fdir->lo_start = 0;
      }
 
      break;
    }
  }

  // // aditi edit: extend the dir if it doesn't have space for another file
  // if (dir_found == -1) {
  //   // No free directory entry found, need to extend the directory
  //   printk("No free directory entries, extending directory to new cluster\n");
    
  //   // Calculate current size of directory in bytes
  //   uint32_t dir_size = dir_n * sizeof(fat32_dirent_t);
    
  //   // Calculate how many entries fit in one cluster
  //   uint32_t bytes_per_cluster = fs->sectors_per_cluster * boot_sector.bytes_per_sec;
  //   uint32_t entries_per_cluster = bytes_per_cluster / sizeof(fat32_dirent_t);
    
  //   // Allocate a new cluster worth of directory entries
  //   uint32_t new_dir_n = dir_n + entries_per_cluster;
  //   fat32_dirent_t *new_fat_dirents = kmalloc(new_dir_n * sizeof(fat32_dirent_t));
    
  //   // Copy existing entries
  //   memcpy(new_fat_dirents, fat_dirents, dir_n * sizeof(fat32_dirent_t));
    
  //   // Initialize new entries as free
  //   for (uint32_t i = dir_n; i < new_dir_n; i++) {
  //       memset(&new_fat_dirents[i], 0, sizeof(fat32_dirent_t));
  //       new_fat_dirents[i].filename[0] = 0xE5; // Mark as free
  //   }
    
  //   // Now use the first new entry for our file
  //   dir_found = dir_n; // Index of first new entry
  //   fat32_dirent_t *fdir = &new_fat_dirents[dir_found];
    
  //   // Set up the entry
  //   fat32_dirent_set_name(fdir, filename);
  //   fdir->attr = is_dir ? FAT32_DIR : FAT32_ARCHIVE;
  //   fdir->file_nbytes = 0;
  //   fdir->hi_start = 0;
  //   fdir->lo_start = 0;
    
  //   // Find a new cluster to extend the directory
  //   uint32_t last_cluster = directory->cluster_id;
    
  //   // Find the end of the directory cluster chain
  //   while (fat32_fat_entry_type(fs->fat[last_cluster]) != LAST_CLUSTER) {
  //       last_cluster = fs->fat[last_cluster];
  //   }
    
  //   // Allocate a new cluster
  //   uint32_t new_cluster = find_free_cluster(fs, 0);
    
  //   // Link the last cluster to the new one
  //   fs->fat[last_cluster] = new_cluster;
  //   fs->fat[new_cluster] = LAST_CLUSTER;
    
  //   // Write the updated FAT to disk
  //   write_fat_to_disk(fs);
    
  //   // Write the extended directory to the disk
  //   write_cluster_chain(fs, directory->cluster_id, (uint8_t *)new_fat_dirents, new_dir_n * sizeof(fat32_dirent_t));
    
  //   // Free original dirents and update with new extended directory
  //   fat_dirents = new_fat_dirents;
  //   dir_n = new_dir_n;
  // }

  // TODO: write out the updated directory to the disk
  // joe comment: [do the math] figure out sector num i of entry, and divide by number of sectors in cluster. then jst write back that specific cluster
  // figure out which sector and just write that one back
  write_fat_to_disk(fs);

  printk("dir cluster id is : %d\n", directory->cluster_id);
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)fat_dirents, dir_n * sizeof(fat32_dirent_t));

  // fat32_dirent_t *dir_check = get_dirents(fs, directory->cluster_id, &dir_n);
  // printk(">>>>Debug: Checking directory after writing, found entries=%d\n", dir_n);
  // for (int i = 0; i < dir_n; i++) {
  //     printk("    Entry %d: %s\n", i, dir_check[i].filename);
  // }
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

  printk("  in write: updating dir size\n");
  fat32_dirent_t *dirent_to_write = &dirents[dirent_index];
  dirent_to_write->file_nbytes = file->n_data;
  if (dirent_to_write->lo_start == 0 && dirent_to_write->hi_start == 0) {
    trace("empty file, allocating new cluster\n");
    uint32_t new_cluster = find_free_cluster(fs, 0);
    dirent_to_write->lo_start = new_cluster & 0xFFFF;
    dirent_to_write->hi_start = (new_cluster >> 16) & 0xFFFF;
    fs->fat[new_cluster] = LAST_CLUSTER;
  }

  printk("  in write: cluster chain for file data\n");
  // write out file data
  write_cluster_chain(fs, dirent_to_write->lo_start | (dirent_to_write->hi_start << 16), file->data, file->n_data);


  printk("  in write: cluster chain for dir entries\n");
  // write out directory entries
  write_cluster_chain(fs, directory->cluster_id, (uint8_t *)dirents, dir_n * sizeof(fat32_dirent_t));

  printk("  in write: fat to disk\n");
  // write out fat
  write_fat_to_disk(fs);
  return 1;

}

int fat32_flush(fat32_fs_t *fs) {
  demand(init_p, "fat32 not initialized!");
  // no-op
  return 0;
}