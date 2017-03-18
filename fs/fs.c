#include <inc/string.h>

#include "fs.h"

// ------------------------------------------------------
// Free block bitmap
// ------------------------------------------------------

// Check to see if the block bitmap indicates that block 'blockno' is free.
// Return 1 if the block is free, 0 if not.
bool
block_is_free(uint32_t blockno)
{
	if (super == 0 || blockno >= super->s_nblocks) {
		return 0;
	}
	if (bitmap[blockno / 32] & (1 << (blockno % 32))) {
		return 1;
	}
	return 0;
}

// Initialize the file system
void
fs_init(void)
{
	static_assert(sizeof(struct File) == 256);

	// Find a Yu-OS disk. Use the second IDE disk (number 1) if available
	if (ide_probe_disk1()) {
		ide_set_disk(1);
	} else {
		ide_set_disk(0);
	}
	bc_init();
}