// Main public header file for our user-land support library,
// whose code lives in the lib directory.
// This library is roughly our OS's version of a standard C library,
// and is intended to be linked into all user-mode applications
// (NOT the kernel or boot loader).

#ifndef YUOS_INC_LIB_H
#define YUOS_INC_LIB_H 1

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>
#include <inc/string.h>
#include <inc/error.h>
#include <inc/assert.h>
#include <inc/env.h>
#include <inc/memlayout.h>
#include <inc/syscall.h>

// main user program
void	umain(int argc, char **argv);

// libmain.c or entry.S
extern const char *binaryname;
extern const volatile struct Env *thisenv;
extern const volatile struct Env envs[NENV];
extern const volatile struct PageInfo pages[];

// exit.c
void 	exit(void);

// syscall.c
void sys_cputs(const char *string, size_t len);
envid_t sys_getenvid(void);
int sys_env_destroy(envid_t);
static envid_t sys_exofork(void);
int sys_env_set_status(envid_t env, int status);
int sys_page_alloc(envid_t env, void *pg, int perm);
int sys_page_map(envid_t src_env, void *src_pg,
				envid_t dst_env, void *dst_pg, int perm);
int sys_page_unmap(envid_t env, void *pg);
void sys_yield(void);

// This must be inlined.
static __inline envid_t __attribute__((always_inline))
sys_exofork(void)
{
	envid_t ret;
	__asm __volatile("int %2"
		: "=a" (ret)
		: "a" (SYS_exofork),
		  "i" (T_SYSCALL)
	);
	return ret;
}

#endif 	/* YUOS_INC_LIB_H */