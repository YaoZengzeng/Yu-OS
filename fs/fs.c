#include <inc/string.h>

#include "fs.h"

// ------------------------------------------------------
// Super block
// ------------------------------------------------------

// Validate the first system super-block.
void
check_super(void)
{
	if (super->s_magic != FS_MAGIC) {
		panic("bad file system magic number");
	}

	if (super->s_nblocks > DISKSIZE/BLKSIZE) {
		panic("file system is too large");
	}

	cprintf("superblock is good\n");
}

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

// Mark a block free in the bitmap
void
free_block(uint32_t blockno)
{
	// Blockno zero is the null pointer of block numbers.
	if (blockno == 0) {
		panic("attempt to free zero block");
	}
	bitmap[blockno/32] |= 1<<(blockno%32);
}

// Search the bitmap for a free block and allocate it. When we
// allocate a block, immediately flush the changed bitmap block
// to the disk.
//
// Return block number allocated on success.
// -E_NO_DISK if we are out of block.
int
alloc_block(void)
{
	// The bitmap consists of one or more blocks. A single bitmap block
	// contains the in-use bits for BLKBITSIZE blocks. There are
	// super->s_nblocks blocks in the disk all together.
	int i;

	for (i = 1; i < super->s_nblocks; i++) {
		if (bitmap[i/32] & (1 << (i % 32))) {
			bitmap[i/32] &= ~(1 << (i % 32));
			flush_block((void *)bitmap);
			return i;
		}
	}

	return -E_NO_DISK;
}

// Validate the file system bitmap.
//
// Check that all reserved blocks -- 0, 1, and bitmap blocks themselves --
// are all marked as in-use.
void
check_bitmap(void)
{
	uint32_t i;

	// Make sure all bitmap blocks are marked in-use
	for (i = 0; i * BLKBITSIZE < super->s_nblocks; i++) {
		assert(!block_is_free(2+i));
	}

	// Make sure the reserved and root blocks are marked in-use.
	assert(!block_is_free(0));
	assert(!block_is_free(1));

	cprintf("bitmap is good\n");
}

// ---------------------------------------------------------------
// File system structures
// ---------------------------------------------------------------

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

	// Set "super" to point to the super block.
	super = diskaddr(1);
	check_super();

	// Set "bitmap" to the beginning of the first bitmap block.
	bitmap = diskaddr(2);
	check_bitmap();
}