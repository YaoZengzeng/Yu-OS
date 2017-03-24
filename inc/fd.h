// Public definitions for the POSIX-like file descriptor emulation layer
// that our user-land support library implements for the user of applications.
// See the code in the lib directory for the implementation details.

#ifndef YUOS_INC_FD_H
#define YUOS_INC_FD_H

#include <inc/types.h>
#include <inc/fs.h>

struct Fd;
struct Stat;

// Per-device-class file descriptor operations
struct Dev {
	int dev_id;
	int (*dev_stat)(struct Fd *fd, struct Stat *stat);
	ssize_t (*dev_read)(struct Fd *fd, void *buf, size_t len);
	ssize_t (*dev_write)(struct Fd *fd, const void *buf, size_t len);
	int (*dev_close) (struct Fd *fd);
};

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

struct Stat {
	char st_name[MAXNAMELEN];
	off_t st_size;
	int st_isdir;
	struct Dev *st_dev;
};

extern struct Dev devfile;

#endif /* YUOS_INC_FD_H */
