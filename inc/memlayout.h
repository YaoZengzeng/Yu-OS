#ifndef YUOS_INC_MEMLAYOUT_H
#define YUOS_INC_MEMLAYOUT_H

#ifndef __ASSEMBLER__
#include <inc/types.h>
#include <inc/mmu.h>
#endif 	/* not __ASSEMBLER__ */

/*
 * This file contains definitions for memory management in our OS,
 * which are relevant to both the kernel and user-mode software.
 */

// All physical memory mapped at this address
#define KERNBASE		0xF0000000

// Kernel stack.
#define KSTACKTOP	KERNBASE
#define KSTKSIZE 	(8*PGSIZE)		// size of a kernel stack
#define KSTKGAP		(8*PGSIZE)		// size of a kernel stack guard

#ifndef __ASSEMBLER__
typedef uint32_t	pte_t;
typedef uint32_t	pde_t;
#endif /* !__ASSEMBLER__ */

#endif /* !YUOS_INC_MEMLAYOUT_H */