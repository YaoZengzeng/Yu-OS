#ifndef YUOS_KERN_TRAP_H
#define YUOS_KERN_TRAP_H

#include <inc/trap.h>
#include <inc/mmu.h>

/* The kernel's interrupt descriptor table */
extern struct Gatedesc idt[];
extern struct Pseudodesc idt_pd;

void trap_init(void);

#endif	/* YUOS_KERN_TRAP_H */