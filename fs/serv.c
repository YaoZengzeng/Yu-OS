/*
 * File system server main loop -
 * servers IPC requests from other environments.
 */

#include <inc/x86.h>

#include "fs.h"

// The file system server maintains three structures
// for each open file.
//
// 1. The on-disk 'struct File' is mapped into the part of memory
//	  that maps the disk. This memory is kept private to the file
//	  server.
// 2. Each open file has a 'struct Fd' as well, which sort of
//	  corresponds to a Unix file descriptor. This 'struct Fd' is kept
//	  on *its own page* in memory, and it is shared with any
//	  environments that have the file open.
// 3. 'struct OpenFile' links these other two structures, and is kept
//	  private to the file server. The server maintains an array of
//	  all open files, indexed by "file ID". (There can be at most
//	  MAXOPEN files open concurrently.) The clients uses file IDs to
//	  communicate with the server. File IDs are a lot like
//	  environment IDs in the kernel. Use openfile_lookup to translate
//	  file IDs to struct OpenFile.

struct OpenFile {
	uint32_t	o_fileid;	// file id
	struct File *o_file;	// mapped descriptor for open file
	int o_mode;		// open mode
	struct Fd *o_fd;	// Fd page
};

// Max number of open files in the file system at once
#define MAXOPEN		1024
#define FILEVA		0xD0000000

// initialize to force into data section
struct OpenFile opentab[MAXOPEN] = {
	{ 0, 0, 1, 0}
};

void
serve_init(void)
{
	int i;
	uintptr_t va = FILEVA;
	for (i = 0; i < MAXOPEN; i++) {
		opentab[i].o_fileid = i;
		opentab[i].o_fd = (struct Fd*)va;
		va += PGSIZE;
	}
}

void
umain(int argc, char **argv)
{
	static_assert(sizeof(struct File) == 256);
	binaryname = "fs";
	cprintf("FS is running\n");

	// Check that we are able to do I/O
	outw(0x8A00, 0x8A00);
	cprintf("FS can do I/O\n");

	serve_init();
	fs_init();
}