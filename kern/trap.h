#ifndef YUOS_KERN_TRAP_H
#define YUOS_KERN_TRAP_H

#include <inc/trap.h>

void trap_init(void);
void trap_init_percpu(void);
void print_trapframe(struct Trapframe *tf);
void print_regs(struct PushRegs *regs);

#endif /* YUOS_KERN_TRAP_H */