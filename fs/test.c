#include <inc/x86.h>
#include <inc/string.h>

#include "fs.h"

static char *msg = "This is the NEW message of the day!\n\n";

void
fs_test(void)
{
	struct File *f;
	int r;
	char *blk;
	uint32_t *bits;

	// back up bitmap
	if ((r = sys_page_alloc(0, (void*)PGSIZE, PTE_P|PTE_U|PTE_W)) < 0) {
		panic("in fs_test sys_page_alloc: %e", r);
	}
	bits = (uint32_t*)PGSIZE;
	memmove(bits, bitmap, PGSIZE);
	// allocate block
	if ((r = alloc_block()) < 0) {
		panic("in fs_test alloc_block: %e", r);
	}
	// check that block was free
	assert(bits[r/32] & (1 << (1 << (r % 32))));
	// and is not free any more
	assert(!(bitmap[r/32] & (1 << (r % 32))));
	cprintf("alloc_block is good\n");

	if ((r = file_open("/not-found", &f)) < 0 && r != -E_NOT_FOUND) {
		panic("file_open /not-found: %e", r);
	} else if (r == 0) {
		panic("file_open /not-found succeeded!");
	}
	if ((r = file_open("/newmotd", &f)) < 0) {
		panic("file_open /newmotd: %e", r);
	}
	cprintf("file_open is good\n");
}