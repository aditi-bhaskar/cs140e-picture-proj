#include "rpi.h"
#include "pi-sd.h"
#include "mbr.h"

#define SECTOR_SIZE 512

mbr_t *mbr_read() {
  // Be sure to call pi_sd_init() before calling this function!

  // TODO: Read the MBR into a heap-allocated buffer.  Use `pi_sd_read` or
  // `pi_sec_read` to read 1 sector from LBA 0 into memory.
  uint32_t lba = 0; // first logical block address
  uint32_t nsec = 1; // number of sectors - read one sector
  void *buffer = pi_sec_read(lba, nsec);

  // TODO: Verify that the MBR is valid. (see mbr_check)
  mbr_check((struct mbr *)buffer);

  // TODO: Return the MBR.
  return (struct mbr *)buffer;
}
