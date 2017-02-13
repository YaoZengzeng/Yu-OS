#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/env.h>

static struct Taskstate ts;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapfram and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table. (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};

static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0])) {
		return excnames[trapno];
	}
	if (trapno == T_SYSCALL) {
		return "System call";
	}

	return "(unknown trap)";
}

void
trap_init(void)
{
	extern struct Segdesc gdt[];

	extern void trap_divide();
	extern void trap_debug();
	extern void trap_nmi();
	extern void trap_brkpt();
	extern void trap_oflow();
	extern void trap_bound();
	extern void trap_illop();
	extern void trap_device();
	extern void trap_dblflt();
	extern void trap_tss();
	extern void trap_segnp();
	extern void trap_stack();
	extern void trap_gpflt();
	extern void trap_pgflt();
	extern void trap_fperr();
	extern void trap_align();
	extern void trap_mchk();
	extern void trap_simderr();

	SETGATE(idt[T_DIVIDE], 	1, GD_KT, trap_divide, 	3);
	SETGATE(idt[T_DEBUG], 	1, GD_KT, trap_debug, 	3);
	SETGATE(idt[T_NMI], 	1, GD_KT, trap_nmi, 	3);
	SETGATE(idt[T_BRKPT], 	1, GD_KT, trap_brkpt, 	3);
	SETGATE(idt[T_OFLOW], 	1, GD_KT, trap_oflow, 	3);
	SETGATE(idt[T_BOUND], 	1, GD_KT, trap_bound, 	3);
	SETGATE(idt[T_ILLOP], 	1, GD_KT, trap_illop, 	3);
	SETGATE(idt[T_DEVICE], 	1, GD_KT, trap_device, 	3);
	SETGATE(idt[T_DBLFLT], 	1, GD_KT, trap_dblflt, 	3);
	SETGATE(idt[T_TSS], 	1, GD_KT, trap_tss, 	3);
	SETGATE(idt[T_SEGNP], 	1, GD_KT, trap_segnp, 	3);
	SETGATE(idt[T_STACK], 	1, GD_KT, trap_stack, 	3);
	SETGATE(idt[T_GPFLT], 	1, GD_KT, trap_gpflt, 	3);
	SETGATE(idt[T_PGFLT], 	1, GD_KT, trap_pgflt, 	3);
	SETGATE(idt[T_FPERR], 	1, GD_KT, trap_fperr, 	3);
	SETGATE(idt[T_ALIGN], 	1, GD_KT, trap_align, 	3);
	SETGATE(idt[T_MCHK], 	1, GD_KT, trap_mchk, 	3);
	SETGATE(idt[T_SIMDERR], 1, GD_KT, trap_simderr, 3);

	// Per-CPU setup
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// Initialize the TSS slot of the gdt.
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
						sizeof(struct Taskstate) - 1, 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(GD_TSS0);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p\n", tf);
	print_regs(&tf->tf_regs);
	cprintf("  es    0x----%04x\n", tf->tf_es);
	cprintf("  ds    0x----%04x\n", tf->tf_ds);
	cprintf("  trap  0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT) {
		cprintf("  cr2 0x%08x\n", rcr2());
	}
	cprintf("  err   0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occured in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT) {
		cprintf("  [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write": "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	} else {
		cprintf("\n");
	}
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  es    0x%08x\n", tf->tf_esp);
		cprintf("  ss    0x----%04x\n", tf->tf_ss);
	}
}
void
print_regs(struct PushRegs *regs)
{
	cprintf("  edi   0x%08x\n", regs->reg_edi);
	cprintf("  esi   0x%08x\n", regs->reg_esi);
	cprintf("  ebp   0x%08x\n", regs->reg_ebp);
	cprintf("  oesp  0x%08x\n", regs->reg_oesp);
	cprintf("  ebx   0x%08x\n", regs->reg_ebx);
	cprintf("  edx   0x%08x\n", regs->reg_edx);
	cprintf("  ecx   0x%08x\n", regs->reg_ecx);
	cprintf("  eax   0x%08x\n", regs->reg_eax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	panic("trap_dispatch not finished yet\n");
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Check that interrupts are disabled. If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	cprintf("Incoming TRAP frame at %p\n", tf);

	if ((tf->tf_cs & 3) == 3) {
		// Trap from user mode.
		assert(curenv);

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information,
	last_tf = tf;

	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// Return to the current environment, which should be running
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
}
