#ifndef YUOS_INC_MMU_H
#define YUOS_INC_MMU_H

/*
 * This file contains definitions for the x86 memory unit (MMU),
 * including paging- and segmentation-related data structures and constants,
 * the %cr0, %cr4, and %eflags registers, and traps.
 */

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