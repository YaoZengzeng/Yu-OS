#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/trap.h>
#include <kern/pmap.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/cpu.h>
#include <kern/picirq.h>

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
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16) {
		return "Hardware Interrupt";
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
	extern void trap_syscall();
	extern void irq_timer();

	SETGATE(idt[T_DIVIDE], 	0, GD_KT, trap_divide, 	3);
	SETGATE(idt[T_DEBUG], 	0, GD_KT, trap_debug, 	3);
	SETGATE(idt[T_NMI], 	0, GD_KT, trap_nmi, 	3);
	SETGATE(idt[T_BRKPT], 	0, GD_KT, trap_brkpt, 	3);
	SETGATE(idt[T_OFLOW], 	0, GD_KT, trap_oflow, 	3);
	SETGATE(idt[T_BOUND], 	0, GD_KT, trap_bound, 	3);
	SETGATE(idt[T_ILLOP], 	0, GD_KT, trap_illop, 	3);
	SETGATE(idt[T_DEVICE], 	0, GD_KT, trap_device, 	3);
	SETGATE(idt[T_DBLFLT], 	0, GD_KT, trap_dblflt, 	3);
	SETGATE(idt[T_TSS], 	0, GD_KT, trap_tss, 	3);
	SETGATE(idt[T_SEGNP], 	0, GD_KT, trap_segnp, 	3);
	SETGATE(idt[T_STACK], 	0, GD_KT, trap_stack, 	3);
	SETGATE(idt[T_GPFLT], 	0, GD_KT, trap_gpflt, 	3);
	SETGATE(idt[T_PGFLT], 	0, GD_KT, trap_pgflt, 	3);
	SETGATE(idt[T_FPERR], 	0, GD_KT, trap_fperr, 	3);
	SETGATE(idt[T_ALIGN], 	0, GD_KT, trap_align, 	3);
	SETGATE(idt[T_MCHK], 	0, GD_KT, trap_mchk, 	3);
	SETGATE(idt[T_SIMDERR], 0, GD_KT, trap_simderr, 3);
	SETGATE(idt[T_SYSCALL],	0, GD_KT, trap_syscall, 3);
	SETGATE(idt[IRQ_OFFSET + IRQ_TIMER], 0, GD_KT, irq_timer, 3);

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

	// Handle system call
	if (tf->tf_trapno == T_SYSCALL) {
		uint32_t ret;
		ret = syscall(tf->tf_regs.reg_eax, tf->tf_regs.reg_edx, tf->tf_regs.reg_ecx, \
			tf->tf_regs.reg_ebx, tf->tf_regs.reg_edi, tf->tf_regs.reg_esi);
		curenv->env_tf.tf_regs.reg_eax = ret;
		return;
	}

	// Handle page fault
	if (tf->tf_trapno == T_PGFLT) {
		page_fault_handler(tf);
		return;
	}

	// Handle clock interrupts. Don't forget to acknowledge the
	// interrupt using lapic_eoi() before calling to the scheduler!
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
		lapic_eoi();
		sched_yield();
	}

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT) {
		panic("unhandled trap in kernel");
	} else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	//print_trapframe(tf);
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Check that interrupts are disabled. If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

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

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense
	if (curenv && curenv->env_status == ENV_RUNNING) {
		env_run(curenv);
	} else {
		sched_yield();
	}
}

void
page_fault_handler(struct Trapframe *tf)
{
	uint32_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.

	if (fault_va >= UTOP) {
		panic("page_fault_handler fault_va >= UTOP");
		goto fail;
	}

	// We've already handled kernel-mode exception, so if we get there,
	// the page fault happened in user mode.

	// Call the environment's page fault upcall, if one exists. Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return. In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free. In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	//
	// Hints:
	//		To change what the user environment runs, modify 'curenv->env_tf'
	//		(the 'tf' variable points at 'curenv->env_tf')
	if (curenv->env_pgfault_upcall == NULL) {
		panic("page_fault_handler curenv->env_pgfault_upcall is NULL");
		goto fail;
	}

	struct UTrapframe u;
	u.utf_fault_va = fault_va;
	u.utf_err = tf->tf_err;
	u.utf_regs = tf->tf_regs;
	u.utf_eip = tf->tf_eip;
	u.utf_eflags = tf->tf_eflags;
	u.utf_esp = tf->tf_esp;

	if (curenv->env_tf.tf_esp >= UXSTACKTOP - PGSIZE && curenv->env_tf.tf_esp < UXSTACKTOP) {
		curenv->env_tf.tf_esp -= (sizeof(struct UTrapframe) + 4);
		user_mem_assert(curenv, (const void*)(curenv->env_tf.tf_esp), sizeof(struct UTrapframe) + 4, PTE_W);
		*((struct UTrapframe*)(curenv->env_tf.tf_esp)) = u;
	} else {
		curenv->env_tf.tf_esp = UXSTACKTOP - sizeof(struct UTrapframe);
		*((struct UTrapframe*)(curenv->env_tf.tf_esp)) = u;
	}
	curenv->env_tf.tf_eip = (uintptr_t)(curenv->env_pgfault_upcall);
	env_run(curenv);

fail:
	// Destroy the environment that caused the fault.
	cprintf("[%08x] user fault va %08x ip %08x\n",
		curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}
