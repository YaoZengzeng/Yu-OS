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

//#include <inc/types.h>

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


#endif /* !YUOS_INC_MMU_H */