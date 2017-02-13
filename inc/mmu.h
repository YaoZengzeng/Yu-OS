#ifndef YUOS_INC_MMU_H
#define YUOS_INC_MMU_H

/*
 * This file contains definitions for the x86 memory unit (MMU),
 * including paging- and segmentation-related data structures and constants,
 * the %cr0, %cr4, and %eflags registers, and traps.
 */

/*
 * Paging data structures and constants.
 */
// A linear address 'la' has a three-part structure as follows:
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |      Index     |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
//  \---------- PGNUM(la) ----------/
//
// The PDX, PTX, PGOFF, and PGNUM macros decompose linear addresses as shown.
// To construct a linear address la from PDX(la), PTX(la), and PGOFF(la),
// use PGADDR(PDX(la), PTX(la), PGOFF(la)).

// page number field of address
#define PGNUM(la) 		(((uintptr_t) (la)) >> PTXSHIFT)

// page directory index
#define PDX(la) 		((((uintptr_t) (la)) >> PDXSHIFT) & 0x3FF)

// page table index
#define PTX(la) 		((((uintptr_t) (la)) >> PTXSHIFT) & 0x3FF)

// offset in page
#define PGOFF(la) 		(((uintptr_t) (la)) & 0xFFF)

// construct linear addresses from indexes and offset
#define PGADDR(d, t, o) ((void*) ((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// Page directory and page table constants.
#define NPDENTRIES	1024	// page directory entries per page directory
#define NPTENTRIES	1024	// page table entries per page table

#define PGSIZE		4096	// bytes mapped by a page
#define PGSHIFT		12 		// log2(PGSIZE)

#define PTSIZE		(PGSIZE*NPTENTRIES) // bytes mapped by a page directory entry
#define PTSHIFT		22				// log2(PTSIZE)

#define PTXSHIFT	12 			// offset of PTX in a linear address
#define PDXSHIFT	22			// offset of PDX in a linear address

// Page table/directory entry flags.
#define PTE_P		0x001	// Present
#define PTE_W		0x002	// Writeable
#define PTE_U		0x004	// User
#define PTE_PWT		0x008	// Write-Through
#define PTE_PCD		0x010	// Cache-Disable
#define PTE_A		0x020	// Accessed
#define PTE_D		0x040	// Dirty
#define PTE_PS		0x080	// Page Size
#define PTE_G		0x100	// Global

// The PTE_AVAIL bits aren't used by the kernel or interpreted by
// the hardware, so user processes are allowed to set them arbitrarily.
#define PTE_AVAIL 	0xE00	// Available for software use

// Address in page table or page directory entry
#define PTE_ADDR(pte) ((physaddr_t) (pte) & ~0xFFF)

// Control Register flags
#define CR0_PE		0x00000001	// Protection Enable
#define CR0_MP		0x00000002	// Monitor coProcessor
#define CR0_EM		0x00000004	// Emulation
#define CR0_TS		0x00000008	// Task Switched
#define CR0_ET		0x00000010	// Extension Type
#define CR0_NE		0x00000020	// Numeric Errror
#define CR0_WP		0x00010000	// Write Protect
#define CR0_AM		0x00040000	// Alignment Mask
#define CR0_NW		0x20000000	// Not Writethrough
#define CR0_CD		0x40000000	// Cache Disable
#define CR0_PG		0x80000000	// Paging

// Eflags register
#define FL_IF		0x00000200  // Interrupt Flag

 /*
  * Segmentation data structures and constants.
  */

#ifdef __ASSEMBLER__

/*
 * Macros to build GDT entries in assembly.
 * Granularity: 4K, 32-bit segment and non-system segment
 */
#define SEG_NULL 			\
 	.word 0, 0;			\
 	.byte 0, 0, 0, 0		
#define SEG(type, base, lim) 		\
 	.word (((lim) >> 12) & 0xffff), ((base) & 0xffff); 	\
 	.byte (((base) >> 16) & 0xff), (0x90 | (type)),		\
 		(0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#else 	// not __ASSEMBLER__

#include <inc/types.h>

// Segment Descriptors
struct Segdesc {
		unsigned sd_lim_15_0 : 16;  // Low bits of segment limit
 		unsigned sd_base_15_0 : 16; // Low bits of segment base address
 		unsigned sd_base_23_16 : 8; // Middle bits of segment base address
 		unsigned sd_type : 4;		// Segment type (see STS_ constants)
 		unsigned sd_s : 1;			// 0 = system, 1 = application
 		unsigned sd_dpl : 2;		// Descriptor Privilege Level
 		unsigned sd_p : 1;			// Present
 		unsigned sd_lim_19_16 : 4;	// High bits of segment limit
 		unsigned sd_avl : 1;		// Unused (available for software use)
 		unsigned sd_rsv1 : 1;		// Reserved
 		unsigned sd_db : 1;			// 0 = 16-bit segment, 1 = 32-bit segment
 		unsigned sd_g : 1;			// Granularity: limit scaled by 4K when set
 		unsigned sd_base_31_24 : 8;	// High bits of segment base address
};
// Null segment
#define SEG_NULL 		(struct Segdesc){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
// Segment that is loadable but faults when used
// sd_s = 1, sd_p = 1, sd_db = 1
#define SEG_FAULT		(struct Segdesc){ 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0 }
// Normal segment
// application, present, 32-bit segment, granularity 4K
#define SEG(type, base, lim, dpl) (struct Segdesc) 		\
{ ((lim) >> 12) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,	\
    type, 1, dpl, 1, (unsigned) (lim) >> 28, 0, 0, 1, 1,		\
    (unsigned) (base) >> 24 }
// application, present, 32-bit segment, granularity 1byte
#define SEG16(type, base, lim, dpl) (struct Segdesc)			\
{ (lim) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,		\
    type, 1, dpl, 1, (unsigned) (lim) >> 16, 0, 0, 1, 0,		\
    (unsigned) (base) >> 24 }

#endif /* !__ASSEMBLER__ */

// Application segment type bits
#define STA_X 			0x8 		// Executable segment
#define STA_E			0x4 		// Expand down (non-executable segments)
#define STA_C 			0x4			// Conforming code segment (executable only)
#define STA_W			0x2			// Writeable (non-executable segments)
#define STA_R 			0x2			// Readable (executable segments)
#define STA_A			0x1 		// Accessed

// System segment type bits
#define STS_T32A 		0x9 		// Available 32-bit TSS
#define STS_IG32		0xE	    	// 32-bit Interrupt Gate
#define STS_TG32		0xF	    	// 32-bit Trap Gate

#ifndef __ASSEMBLER__

// Task state segment format (as described by the Pentium architecture book)
struct Taskstate {
	uint32_t	ts_link;	// Old ts selector
	uintptr_t 	ts_esp0;	// Stack pointers and segment selectors
	uint16_t 	ts_ss0;		// after an increase in privilege level
	uint16_t	ts_padding1;
	uintptr_t 	ts_esp1;
	uint16_t 	ts_ss1;
	uint16_t	ts_padding2;
	uintptr_t	ts_esp2;
	uint16_t	ts_ss2;
	uint16_t	ts_padding3;
	physaddr_t	ts_cr3;		// Page directory base
	uintptr_t	ts_eip;		// Saved state from last task switch
	uint32_t	ts_eflags;
	uint32_t	ts_eax;		// More saved state (registers)
	uint32_t	ts_ecx;
	uint32_t	ts_edx;
	uint32_t	ts_ebx;
	uintptr_t	ts_esp;
	uintptr_t	ts_ebp;
	uint32_t	ts_esi;
	uint32_t	ts_edi;
	uint16_t	ts_es;	// Even more saved state (segment selectors)
	uint16_t	ts_padding4;
	uint16_t	ts_cs;
	uint16_t	ts_padding5;
	uint16_t	ts_ss;
	uint16_t	ts_padding6;
	uint16_t	ts_ds;
	uint16_t	ts_padding7;
	uint16_t	ts_fs;
	uint16_t	ts_padding8;
	uint16_t	ts_gs;
	uint16_t	ts_padding9;
	uint16_t	ts_ldt;
	uint16_t	ts_padding10;
	uint16_t	ts_t;		// Trap on task switch
	uint16_t	ts_iomb;	// I/O map base address
};

// Gate descriptors for interrupts and traps
struct Gatedesc {
	unsigned gd_off_15_0 : 16;		// low 16 bits of offset in segment
	unsigned gd_sel : 16;			// segment selector
	unsigned gd_args : 5;			// # args, 0 for interrupt/trap gates
	unsigned gd_rsv1 : 3;			// reserved(should be zero I guess)
	unsigned gd_type : 4;			// type(STS_{TG, IG32, TG32})
	unsigned gd_s : 1;				// must be 0 (system)
	unsigned gd_dpl : 2;			// descriptor(meaning now) privilege level
	unsigned gd_p : 1;				// Present
	unsigned gd_off_31_16 : 16;		// high bits of offset in segment
};

// Set up a normal interrupt/trap gate descriptor.
// - istrap: 1 for a trap (=exception) gate, 0 for an interrupt gate.
	//	The difference between an interrupt gate and a trap gate is in
	//	the effect on IF (the interrupt-enable flag). An interrupt that
	// 	vectors through an interrupt gate resets IF, thereby preventing other
	//	interrupts from interfering with the current interrupt handler. A subsequent
	//	IRET instruction restores IF to the value in the EFLAGS image on the stack.
	//	An interrupt through a trap gate does not change IF.
// - sel: Code segment selector for interrupt/trap handler
// - off: Offset in code segment for interrupt/trap handler
// - dpl: Descriptor Privilege Level -
//		the privilege level required for software to invoke
//		this interrupt/trap gate explicitly using an int instruction
#define SETGATE(gate, istrap, sel, off, dpl) 			\
{						\
	(gate).gd_off_15_0 = (uint32_t) (off) & 0xffff;		\
	(gate).gd_sel = (sel);				\
	(gate).gd_args = 0;				\
	(gate).gd_rsv1 = 0;				\
	(gate).gd_type = (istrap) ? STS_TG32 : STS_IG32;	\
	(gate).gd_s = 0;				\
	(gate).gd_dpl = (dpl);			\
	(gate).gd_p = 1;			\
	(gate).gd_off_31_16 = (uint32_t) (off) >> 16;	\
}

// Pseudo-descriptors used for LGDT, LLDT and LIDT instructions.
struct Pseudodesc {
	uint16_t pd_lim;		// Limit
	uint32_t pd_base;		// Base address
} __attribute__ ((packed));

#endif /* !__ASSEMBLER__ */

#endif /* !YUOS_INC_MMU_H */