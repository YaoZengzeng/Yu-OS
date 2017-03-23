#include <inc/lib.h>

const char *msg = "This is the NEW message of the day!\n\n";

#define FVA ((struct Fd*)0xCCCCC000)

static int
xopen(const char *path, int mode)
{
	extern union Fsipc fsipcbuf;
	envid_t fsenv;

	strcpy(fsipcbuf.open.req_path, path);
	fsipcbuf.open.req_omode = mode;

	fsenv = ipc_find_env(ENV_TYPE_FS);
	ipc_send(fsenv, FSREQ_OPEN, &fsipcbuf, PTE_P | PTE_W | PTE_U);
	return ipc_recv(NULL, FVA, NULL);
}

void
umain(int argc, char **argv)
{
	int r, f, i;
	struct Stat st;
	char buf[512];

	// We open files manually first, to avoid the FD layer
	if ((r = xopen("/not-found", O_RDONLY)) < 0 && r != -E_NOT_FOUND) {
		panic("serve_open /not-found failed: %e", r);
	} else if (r >= 0) {
		panic("serve_open /not-found succeeded!\n");
	}

	if ((r = xopen("/newmotd", O_RDONLY)) < 0) {
		panic("serve_open /newmotd failed: %e", r);
	}
	if (FVA->fd_dev_id != 'f' || FVA->fd_offset != 0 || FVA->fd_omode != O_RDONLY) {
		panic("serve_open did not fill struct Fd correctly\n");
	}
	cprintf("serve_open is good\n");

	if ((r = devfile.dev_stat(FVA, &st)) < 0) {
		panic("file_stat failed: %e", r);
	}
	if (strlen(msg) != st.st_size) {
		panic("file_stat returned size %d wanted %d\n", st.st_size, strlen(msg));
	}
	cprintf("file_stat is good\n");

	memset(buf, 0, sizeof(buf));
	if ((r = devfile.dev_read(FVA, buf, sizeof(buf))) < 0) {
		panic("file_read failed: %e", r);
	}
	if (strcmp(buf, msg) != 0) {
		panic("file_read returned wrong data");
	}
	cprintf("file_read is good\n");
}
