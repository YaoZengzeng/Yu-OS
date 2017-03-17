/*
 * File system server main loop -
 * servers IPC requests from other environments.
 */

#include <inc/x86.h>

#include "fs.h"

void
umain(int argc, char **argv)
{
	binaryname = "fs";
	cprintf("FS is running\n");

	// Check that we are able to do I/O
	outw(0x8A00, 0x8A00);
	cprintf("FS can do I/O\n");
}