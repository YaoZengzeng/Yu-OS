#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/monitor.h>

struct Env *envs = NULL;		// All environments
struct Env *curenv	= NULL;		// The current env
static struct Env *env_free_list;	// Free environment list

#define ENVGENSHIFT 12		// >= LOGNENV

// Global descriptor table.
//
// Set up global descriptor table (GDT) with seperate segments for
// kernel mode and user mode. Segments serve many purposes on the x86.
// We don't use any of their memory-mapping capabilities, but we need
// them to switch privilege levels.
//
// The kernel and user segments are identical except for the DPL.
// To load the SS register, the CPL must equal the DPL. Thus,
// we must duplicate the segments for the user and the kernel.
//
// In particular, the last argument to the SEG macro used in the
// definition of gdt specifies the Descriptor Privilege Level (DPL)
// of that descriptor: 0 for kernel and 3 for user.
//
struct Segdesc gdt[] =
{
	// 0x0 - unused (always faults -- for trapping NULL for pointers)
	SEG_NULL,

	// 0x8 - kernel code segment
	[GD_KT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 0),

	// 0x10 - kernel data segment
	[GD_KD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 0),

	// 0x18 - user code segment
	[GD_UT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 3),

	// 0x20 - user data segment
	[GD_UD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 3),

	// 0x28 - tss, initialized in trap_init_percpu()
	[GD_TSS0 >> 3] = SEG_NULL,
};

struct Pseudodesc gdt_pd = {
	sizeof(gdt) - 1, (unsigned long) gdt
};

// Mark all environments in 'envs' as free, set their env_ids to 0,
// and insert them into the env_free_list.
// Make sure the environments are in the free list in the same order
// they are in the envs array (i.e., so that the first call to
// env_alloc() returns envs[0])
//
void
env_init(void)
{
	int i;

	// Set up envs array
	for (i = NENV - 1; i >= 0; i--) {
		envs[i].env_link = env_free_list;
		env_free_list = &envs[i];

		envs[i].env_status = ENV_FREE;
	}

	// Per-CPU part of the initialization
	env_init_percpu();
}

// Load GDT and segment descriptors.
void
env_init_percpu(void)
{
	lgdt(&gdt_pd);
	// The kernel never uses GS or FS, so we leave those set to
	// the user data segment.
	asm volatile("movw %%ax,%%gs" :: "a" (GD_UD|3));
	asm volatile("movw %%ax,%%fs" :: "a" (GD_UD|3));
	// The kernel does use ES, DS, and SS. We'll change between
	// the kernel and user data segments as needed.
	asm volatile("movw %%ax,%%es" :: "a" (GD_KD));
	asm volatile("movw %%ax,%%ds" :: "a" (GD_KD));
	asm volatile("movw %%ax,%%ss" :: "a" (GD_KD));
	// Load the kernel text segment into CS.
	asm volatile("ljmp %0,$1f\n 1:\n" :: "i" (GD_KT));
	// For good measure, clear the local descriptor table (LDT),
	// since we don't use it.
	lldt(0);
}

//
// Allocate len bytes of physical memory for environment env,
// and map it at virtual address va in the environment's address space.
// Does not zero or otherwise initialize the mapped pages in any way.
// Pages should be writable by user and kernel.
// Panic if any allocation attempts fails.
//
static void
region_alloc(struct Env *e, void *va, size_t len)
{
	//	It is easier to use region_alloc if the caller can pass
	//	'va' and 'len' values that are not page-aligned.
	//	We should round va down, and round (va + len) up.
	//	(Watch out for corner-cases)
	size_t start, end, i;
	struct PageInfo *page;
	int r, perm;

	start = ROUNDDOWN(va, PGSIZE);
	end = ROUNDUP((size_t)va + len, PGSIZE);

	perm = PTE_P|PTE_W|PTE_U;
	for (i = start; i < end; i += PGSIZE) {
		page = page_alloc(0);
		if (page == NULL) {
			panic("region_alloc: page_alloc get page failed");
		}
		r = page_insert(env->env_pgdir, page, i, perm);
		if (r != 0) {
			panic("region_alloc: page_insert insert page failed");
		}
	}
}

//
// Set up the initial program binary, stack, and processor flags
// for a user process.
// This function is ONLY called during kernel initialization,
// before running the first user-mode environment.
//
// This function loads all loadable segments from the ELF binary image
// into the environment's user memory, starting at the appropriate
// virtual addresses indicated in the ELF program header.
// At the same time it clears to zero any portions of these segments
// that are marked in the program header as being mapped
// but not actually present in the ELF file - i.e., the program's bss section.
//
// All this is very similar to what our boot loader does, except the boot
// loader also needs to read the code from disk.
//
// Finally, this function maps one page for the program's initial stack.
//
// load_icode panics if it encouters problems.
//
static void
load_icode(struct Env *e, uint8_t *binary)
{
	//	Load each program segment into virtual memory
	//	at the address specified in the ELF section header.
	//	We should only load segments with ph->p_type == ELF_PROG_LOAD.
	//	Each segment's virtual address can be found in ph->p_va
	//	and its size in memory can be found in ph->p_memz.
	//	The ph->p_filesz bytes from the ELF binary, starting at
	//	'binary + ph->p_offset', should be copied to virtual address
	//	ph->p_va. Any remaining memory bytes should be cleared to zero.
	//	(The ELF header should have ph->p_filesz <= ph->p_memz.)
	//	Use function created before to allocate and map pages.
	//
	//	All page protection bits should be user read/write for now.
	//	ELF segments are not necessarily page-aligned, but we can
	//	assume for this function that no two segments will touch
	//	the same virtual page.
	//
	//	function like region_alloc is useful.
	//
	//	Loading the segments is much simpler if we can move data
	//	directly into the virtual addresses stored in the ELF binary.
	//	So e's page directory should be in force during this function.
	//
	//	we must also do something with the program's entry point,
	//	to make sure that the environment starts executing there.
	//	See env_run() and env_pop_tf() below.
	struct Elf *elf;
	struct Proghdr *ph, *eph;
	struct PageInfo *page;

	// switch to e's page directory temporarily
	lcr3(PADDR(e->env_pgdir));

	elf = (struct Elf *) binary;
	ph = (struct Proghdr *) (binary + elf->e_phoff);
	eph = ph + elf->e_phnum;
	for (; ph < eph; ph++) {
		if (ph->p_type != ELF_PROG_LOAD) {
			continue;
		}
		region_alloc(e, ph->p_va, ph->p_memz);
		memset(ph->p_va, ph->p_memz);
		memmove(ph->p_va, binary + ph->p_offset, ph->p_filesz);
	}

	// switch back to kern page directory
	lcr3(PADDR(kern_pgdir));

	//	Now map one page for the program's initial stack
	//	at virtual address USTACKTOP - PGSIZE.
	region_alloc(e, USTACKTOP - PGSIZE, PGSIZE);

	e->env_tf.tf_eip = elf->e_entry;
}

//
// Allocates a new env with env_alloc, loads the named elf
// binary into it with load_icode, and sets its env_type.
// This function is ONLY called during kernel initialization,
// before running the first user-mode environment.
// The new env's parent ID is set to 0.
//
void
env_create(uint8_t *binary, enum EnvType type)
{
	struct Env *e;
	int r;

	if ((r = env_alloc(&e, 0)) < 0) {
		panic("env_create: alloc env failed");
	}

	load_icode(e, binary);

	e->env_type = type;
}

//
// Initialize the kernel virtual memory layout for environment e.
// Allocate a page directory, set e->env_pgdir accordingly,
// and initialize the kernel portion of the new environment's address space.
// Do NOT (yet) map anything into the user portion
// of the environment's virtual address space.
//
// Returns 0 on success, < 0 on error. Errors include:
// -E_NO_MEM if page directory or table could not be allocated.
//
static int
env_setup_vm(struct Env *e)
{
	int i;
	struct PageInfo *p = NULL;

	// Allocate a page for the page directory
	if (!(p = page_alloc(ALLOC_ZERO))) {
		return -E_NO_MEM;
	}

	// Now, set e->env_pgdir and initialize the page directory.
	// - The VA space of all envs is identical above UTOP
	// (except at UVPT, which we've set below).
	// See inc/memlayout.h for permissions and layout.
	// We can use kern_pgdir as a template.
	//		- The initial VA below UTOP is empty.
	//		- We do not need to make any more calls to page_alloc.
	//		- Note: In general, pp_ref is not maintained for
	//	physical pages mapped only above UTOP, but env_pgdir
	//	is an exception -- you need to increment env_pgdir's
	//	pp_ref for env_free to work correctly.
	//		- The functions in kern/pmap.h are handy.
	for (i = PDX(UTOP); i < NPDENTRIES; i++) {
		e->env_pgdir[i] = kern_pgdir[i];
	}

	//	UVPT maps the env's own page table read-only.
	// 	Permission: kernel R, user R
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;

	return 0;
}

//
// Allocates and initializes a new environment.
// On success, the new environment is stored in *newenv_store.
//
// Returns 0 on success, < 0 on failure. Errors include:
// -E_NO_FREE_ENV if all NENVS environments are allocated
// -E_NO_MEM on memory exhaustion
//
int
env_alloc(struct Env **newenv_store, envid_t parent_id)
{
	int32_t generation;
	int r;
	struct Env *e;

	if (!(e = env_free_list)) {
		return -E_NO_FREE_ENV;
	}

	// Allocate and set up the page directory for this environment.
	if ((r = env_setup_vm(e)) < 0) {
		return r;
	}

	// Generate an env_id for this environment.
	generation = (e->env_id + (1 << ENVGENSHIFT)) & ~(NEVN - 1);
	if (generation <= 0) {	// Don't create a negative env_id.
		generation = 1 << ENVGENSHIFT;
	}
	e->env_id = generation | (e - envs);

	// Set the basic status variables.
	e->env_parent_id = parent_id;
	e->env_type = ENV_TYPE_USER;
	e->env_status = ENV_RUNNABLE;
	e->env_runs = 0;

	// Clear out all the saved register state,
	// to prevent the register values
	// of a prior environment inhabiting this Env structure
	// from "leaking" into our new environment.
	memset(&e->env_tf, 0, sizeof(e->env_tf));

	// Set up appropriate initial values for the segment registers.
	// GD_UD is the user date segment selector in the GDT, and
	// GD_UT is the user text segment selector (see inc/memlayout.h).
	// The low 2 bits of each segment register contains the
	// Requestor Privilege Level (RPL); 3 means user mode. When
	// we switch privilege levels, the hardware does various
	// checks involving the RPL and the Descriptor Privilege Level
	// (DPL) stored in the descriptors themselves.
	e->env_tf.tf_ds = GD_UD | 3;
	e->env_tf.tf_es = GD_UD | 3;
	e->env_tf.tf_ss = GD_UD | 3;
	e->env_tf.tf_esp = USTACKTOP;
	e->env_tf.tf_cs = GD_UT | 3;
	// set e->env_tf.tf_eip later.

	// commit the allocation
	env_free_list = e->env_link;
	*newenv_store = e;

	cprintf("[%08x] new env %08x\n", curenv ? curenv->env_id : 0, e->env_id);
	return 0;
}

//
// Restores the register values in the Trapframe with 'iret' instruction.
// This exits the kernel and starts some environment's code.
//
// This function does not return.
//
void
env_pop_tf(struct Trapframe *tf)
{
	__asm __volatile("movl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
		"\tiret"
		: : "g" (tf) : "memory");
	panic("iret failed");  /* mostly to placate the compiler */
}

//
// Context switch from curenv to env e.
// Note: if this is the first call to env_run, curenv is NULL.
//
// This function does not return.
//
void
env_run(struct Env *e)
{
	// Step 1: If this is a context switch (a new environment is running):
	//		1. Set the current environment (if any) back to
	//		   ENV_RUNNABLE if it is ENV_RUNNING
	//		2. Set 'curenv' to the new environment,
	//		3. Set its status to ENV_RUNNING,
	//		4. Update its 'env_runs' counter,
	//		5. Use lcr3() to switch to its address space.
	// Step 2: Use env_pop_tf() to restore the environment's
	//		registers and drop into user mode in the environment.

	// This function loads the new environment's state from
	// e->env_tf. Go back through the code we write above
	// and make sure we have set the relevant parts of
	// e->env_tf to sensible values.
	if (curenv != NULL && curenv->env_status == ENV_RUNNING) {
		curenv->env_status = ENV_RUNNABLE;
	}
	curenv = e;
	curenv->env_status = ENV_RUNNING;
	curenv->env_runs++;
	lcr3(PADDR(curenv->env_pgdir));

	env_pop_tf(e->env_tf);
}