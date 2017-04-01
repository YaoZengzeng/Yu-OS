#include <inc/ns.h>
#include <inc/lib.h>

#define debug 0

// Virtual address at which to receive page mappings containing client requests.
#define REQVA		0x0ffff000
union Nsipc	nsipcbuf	__attribute__((aligned(PGSIZE)));

