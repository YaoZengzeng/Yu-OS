// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW 		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page. If not, panic.
	// Hint:
	//	Use the read-only page table mappings at uvpt
	//	(see <inc/memlayout.h>).
	addr = ROUNDDOWN(addr, PGSIZE);
	if (!(uvpt[(unsigned)addr/PGSIZE] & PTE_P)) {
		r = sys_page_alloc(0, addr, PTE_W | PTE_U | PTE_P);
		if (r != 0) {
			panic("pgfault sys_page_alloc for page not present failed: %e", r);
		}
		return;
	}

	if (!(uvpt[(unsigned)addr/PGSIZE] & PTE_COW)) {
		panic("pgfault faulting access is not a write or copy-on-write page");
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	// 	We should make three system calls.
	r = sys_page_alloc(0, PFTEMP, PTE_W | PTE_U | PTE_P);
	if (r != 0) {
		panic("pgfault sys_page_alloc PFTEMP failed: %e", r);
	}
	memmove(PFTEMP, addr, PGSIZE);
	r = sys_page_unmap(0, addr);
	if (r != 0) {
		panic("pgfault sys_page_unmap addr failed: %e", r);
	}
	r = sys_page_map(0, PFTEMP, 0, addr, PTE_W | PTE_U | PTE_P);
	if (r != 0) {
		panic("pgfault sys_page_map PFTEMP to addr failed: %e", r);
	}
	r = sys_page_unmap(0, PFTEMP);
	if (r != 0) {
		panic("pgfault sys_page_unmap PFTEMP failed: %e", r);
	}
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address. If the page is writeable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	if ((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)) {
		// Must map envid's page first, otherwise something tricky will happen
		r = sys_page_map(0, (void*)(pn*PGSIZE), envid, (void*)(pn*PGSIZE), PTE_P | PTE_U | PTE_COW);
		if (r != 0) {
			panic("duppage sys_page_map 2 failed");
			return r;
		}
		r = sys_page_map(0, (void*)(pn*PGSIZE), 0, (void*)(pn*PGSIZE), PTE_P | PTE_U | PTE_COW);
		if (r != 0) {
			panic("duppage sys_page_map 1 failed");
			return r;
		}
	} else {
		r = sys_page_map(0, (void*)(pn*PGSIZE), envid, (void*)(pn*PGSIZE), PTE_P | PTE_U);
		if (r != 0) {
			panic("duppage sys_page_map 3 failed");
			return r;
		}
	}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Return: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//	Use uvpd, uvpt, and duppage.
//	Remember to fix "thisenv" in the child process.
//	Neither user exception stack should ever be marked copy-on-write,
//	so we must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	envid_t envid;
	uint8_t *addr;
	int r;
	extern unsigned char end[];

	set_pgfault_handler(pgfault);
	// Allocate a new child environment.
	// The kernel will initialize it with a copy of our register state,
	// so that the child will appear to have called sys_exofork() too -
	// except that in the child, this "fake" call to sys_exofork()
	// will return 0 instead of the envid of the child.
	envid = sys_exofork();
	if (envid < 0) {
		cprintf("fork sys_exofork failed: %e\n", envid);
	}
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// We're the parent.
	// Eagerly copy-on-write our entire address space into the child.
	for (addr = (uint8_t*) UTEXT; addr < end; addr += PGSIZE) {
		duppage(envid, (unsigned)addr/PGSIZE);
	}

	// Also copy the stack we are currently running on
	duppage(envid, (unsigned)ROUNDDOWN(&addr, PGSIZE)/PGSIZE);

	// Allocate a new page for the child's user exception stack
	if ((r = sys_page_alloc(envid, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_W | PTE_U)) < 0) {
		cprintf("fork sys_page_alloc failed: %e ");
	}

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0) {
		cprintf("fork sys_env_set_status failed: %e\n", r);
	}

	return envid;
}
