#include <inc/fs.h>
#include <inc/string.h>
#include <inc/lib.h>

union Fsipc fsipcbuf __attribute__((aligned(PGSIZE)));

struct Dev devfile =
{
	.dev_id = 'f'
};
