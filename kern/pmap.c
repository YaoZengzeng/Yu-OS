#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/assert.h>
#include <inc/memlayout.h>
#include <inc/string.h>

#include <kern/pmap.h>
#include <kern/kclock.h>

// These variables are set by i386_detect_memory()
size_t npages;			// Amount of physical memory (in pages)
static size_t npages_basemem;	// Amount of base memory (in pages)

// These variables are set in mem_init()
pde_t *kern_pgdir;		// Kernel's initial page directory
struct PageInfo	*pages;	// Physical page state array
static struct PageInfo *page_free_list;	// Free list of physical pages

// -----------------------------------------------------------------
// Detect machine's physical memory setup.
// -----------------------------------------------------------------

static int
nvram_read(int r)
{
	return mc146818_read(r) | (mc146818_read(r + 1) << 8);
}

static void
i386_detect_memory(void)
{
	size_t npages_extmem;

	// Use CMOS calls to measure available base & extended memory.
	// (CMOS calls return results in kilobytes.)
	npages_basemem = (nvram_read(NVRAM_BASELO) * 1024) / PGSIZE;
	npages_extmem = (nvram_read(NVRAM_EXTLO) * 1024) / PGSIZE;

	// Calculate the number of physical pages available in both base
	// and extended memory.
	if (npages_extmem) {
		npages = (EXTPHYSMEM / PGSIZE) + npages_extmem;
	} else {
		npages = npages_basemem;
	}

	cprintf("Physical memory: %uK available, base = %uK, extended = %uK\n",
		npages * PGSIZE / 1024,
		npages_basemem * PGSIZE / 1024,
		npages_extmem * PGSIZE / 1024);
}

// -----------------------------------------------------------------
// Set up memory mappings above UTOP.
// -----------------------------------------------------------------

static void check_page_free_list(bool only_low_memory);

// This simple physical memory allocator is used only while Yu-OS is setting
// up its virtual memory system. page_alloc() is the real allocator.
//
// If n>0, allocates enough pages of contiguous physcial memory to hold 'n'
// bytes. Doesn't initialize the momory. Returns a kernel virtual address.
//
// If n==0, returns the address of the next free page without allocating
// anything.
// If we're out of memory, boot_alloc should panic.
// This function may ONLY be used during initialization,
// before the page_free_list list has been set up.
static void *
boot_alloc(uint32_t n)
{
	static char *nextfree;		// virtual address of next byte of free memory
	char *result;

	// Initialize next free if this is the first time.
	// 'end' is a magic symbol automatically generated by the linker,
	// which points to the end of the kernel's bss segment:
	// the first virtual address that the linker did *not* assign
	// to any kernel code or global variables.
	if (!nextfree) {
		extern char end[];
		nextfree = ROUNDUP((char *) end, PGSIZE);
	}

	// Allocate a chunck large enough to hold 'n' bytes, then update
	// nextfree. Make sure nextfree is kept aligned
	// to a multiple of PGSIZE.
	result = nextfree;
	n = ROUNDUP(n, PGSIZE);
	nextfree += n;

	return result;
}


// Set up a two-level page table:
// 		kern_pgdir is its linear (virtual) address of th root
//
// This function only sets up the kernel part of the address space
// (ie. addresses >= UTOP). The user part of the address space
// will be setup later.
//
// From UTOP to ULIM, the user is allowed to read but not write.
// Above ULIM the user cannot read or write.
void
mem_init(void)
{
	uint32_t cr0;
	size_t n;

	// Find out how much memory the machine has (npages & npages_basemem)
	i386_detect_memory();

	// create initial page directory.
	kern_pgdir = (pde_t *) boot_alloc(PGSIZE);
	memset(kern_pgdir, 0, PGSIZE);

	// Recursively insert PD in itself as a page table, to form
	// a virtual page table at virtual address UVPT.
	// Permissions: kernel R, user R
	kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;

	// Allocate an array of npages 'struct PageInfo's and store it in 'pages'.
	// The kernel uses this array to keep track of physical pages: for
	// each physical page, there is a corresponding struct PageInfo in this
	// array. 'npages' is the number of physical pages in memory. Use memset
	// to initialize all fields of each struct PageInfo to 0.
	pages = (struct PageInfo *) boot_alloc(npages * sizeof(struct PageInfo));
	memset(pages, 0, npages * sizeof(struct PageInfo));

	// Now that we've allocated the initial kernel data structures, we set
	// up the list of free physical pages. Once we've done so, all further
	// memory management will go through the page_* functions. In
	// particular, we can now map memory using boot_map_region
	// or page_insert
	page_init();

	check_page_free_list(1);
}

// -------------------------------------------------------------
// Tracking of physical pages.
// The 'pages' array has one 'struct PageInfo' entry per physical page.
// Pages are reference counted, and free pages are kept on a linked list.
// --------------------------------------------------------------

//
// Initialize page structure and memory free list.
// After this done, NEVER use boot_alloc again. ONLY use the page
// allocator functions below to allocate an deallocate physical
// memory via the page_free_list.
//
void
page_init(void)
{
	// What memory is free?
	// 	1) Mark physical page 0 as in use.
	//	   This way we preserve the real-mode IDT and BIOS structures
	//	   in case we never need them. (Currently we don't, but...)
	//	2) The rest of base memory, [PGSIZE, npages_basemem * PGSIZE)
	//	   is free.
	//	3) Then comes the IO hole [IOPHYSMEM, EXTPHYSMEM), which must
	//	   never be allocated.
	//	4) Then extended memory [EXTPHYSMEM, ...).
	//	   Some of it is in use, some is free. Where is the kernel
	//	   in physical memory? Which pages are already in use for
	//	   page tables and other data structures?
	// NB: DO NOT actually touch the physical memory corresponding to
	// free pages!
	size_t i;
	for (i = 0; i < npages; i++) {
		// page 0
		if (i == 0) {
			continue;
		}
		// IO hole
		if (page2pa(&pages[i]) >= IOPHYSMEM && page2pa(&pages[i]) < EXTPHYSMEM) {
			continue;
		}
		// kernel, kern_pgdir and pages
		if (page2pa(&pages[i]) >= EXTPHYSMEM && page2pa(&pages[i]) < PADDR(boot_alloc(0))) {
			continue;
		}
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}
}

// --------------------------------------------------------------
// Checking functions.
// --------------------------------------------------------------

//
// Check that the pages on the page_free_list are reasonable.
//
static void
check_page_free_list(bool only_low_memory)
{
	struct PageInfo *pp;
	unsigned pdx_limit = only_low_memory ? 1 : NPDENTRIES;
	int nfree_basemem = 0, nfree_extmem = 0;
	char *first_free_page;

	if (!page_free_list) {
		panic("'page_free_list' is a null pointer!");
	}

	if (only_low_memory) {
		// Move pages with lower addresses first in the free
		// list, since entry_pgdir does not map all pages.
		struct PageInfo *pp1, *pp2;
		struct PageInfo **tp[2] = { &pp1, &pp2 };
		for (pp = page_free_list; pp; pp = pp->pp_link) {
			int pagetype = PDX(page2pa(pp)) >= pdx_limit;
			*tp[pagetype] = pp;
			tp[pagetype] = &pp->pp_link;
		}
		*tp[1] = 0;
		*tp[0] = pp2;
		page_free_list = pp1;
	}

	// if there's a page that shouldn't be on the free list,
	// try to make sure it eventually causes trouble
	for (pp = page_free_list; pp; pp = pp->pp_link) {
		if (PDX(page2pa(pp)) < pdx_limit) {
			memset(page2kva(pp), 0x97, 128);
		}
	}

	first_free_page = (char *) boot_alloc(0);
	for (pp = page_free_list; pp; pp = pp->pp_link) {
		// check that we didn't corrupt the free list itself
		assert(pp >= pages);
		assert(pp < pages + npages);
		assert(((char *) pp - (char *) pages) % sizeof(*pp) == 0);

		// check a few pages that shouldn't be on the free list
		assert(page2pa(pp) != 0);
		assert(page2pa(pp) != IOPHYSMEM);
		assert(page2pa(pp) != EXTPHYSMEM - PGSIZE);
		assert(page2pa(pp) != EXTPHYSMEM);
		assert(page2pa(pp) < EXTPHYSMEM || (char *) page2kva(pp) >= first_free_page);

		if (page2pa(pp) < EXTPHYSMEM) {
			++nfree_basemem;
		} else {
			++nfree_extmem;
		}
	}

	assert(nfree_basemem > 0);
	assert(nfree_extmem > 0);

	cprintf("check_page_free_list(%d) succeeded!\n", only_low_memory ? 1 : 0);
}