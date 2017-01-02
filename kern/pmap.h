#ifndef YUOS_KERN_PMAP_H
#define YUOS_KERN_PMAP_H

#include <inc/memlayout.h>
#include <inc/assert.h>

extern struct PageInfo *pages;
extern size_t npages;

/* This macro takes a kernel virtual address -- an address that points above
 * KERNBASE, where the machine's maximum 256MB of physical memory is mapped --
 * and returns the corresponding physical address. It panics if you pass it a
 * non-kernel virtual address.
 */
#define PADDR(kva) _paddr(__FILE__, __LINE__, kva)

static inline physaddr_t
_paddr(const char *file, int line, void *kva)
{
	if ((uint32_t)kva < KERNBASE) {
		_panic(file, line, "PADDR called with invalid kva %08lx", kva);
	}
	return (physaddr_t)kva - KERNBASE;
}

/* This macro takes a physical address and returns the corresponding kernel
 * virtual address. It panics if you pass an invalid physical address.
 */
#define KADDR(pa) _kaddr(__FILE__, __LINE__, pa)

static inline void*
_kaddr(const char *file, int line, physaddr_t pa)
{
	if (PGNUM(pa) >= npages) {
		_panic(file, line, "KADDR called with invalid pa %08lx", pa);
	}
	return (void *)(pa + KERNBASE);
}

void mem_init(void);

void page_init(void);

static inline physaddr_t
page2pa(struct PageInfo *pp)
{
	return (pp - pages) << PGSHIFT;
}

static inline void*
page2kva(struct PageInfo *pp)
{
	return KADDR(page2pa(pp));
}

#endif /* YUOS_KERN_PMAP_H */