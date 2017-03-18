// Public definitions for the POSIX-like file descriptor emulation layer
// that our user-land support library implements for the user of applications.
// See the code in the lib directory for the implementation details.

#ifndef YUOS_INC_FD_H
#define YUOS_INC_FD_H

#include <inc/types.h>
#include <inc/fs.h>

struct FdFile {
	int id;
};

struct Fd {
	int fd_dev_id;
	off_t fd_offset;
	int fd_omode;
	union {
		// File server files
		struct FdFile fd_file;
	};
};