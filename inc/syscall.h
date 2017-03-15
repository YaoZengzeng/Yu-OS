#ifndef YUOS_INC_SYSCALL_H
#define YUOS_INC_SYSCALL_H

/* system call numbers */
enum {
	SYS_cputs = 0,
	SYS_getenvid,
	SYS_env_destroy,
	SYS_page_alloc,
	SYS_page_map,
	SYS_page_unmap,
	SYS_exofork,
	SYS_env_set_status,
	SYS_yield,
	SYS_env_set_pgfault_upcall,
	SYS_ipc_try_send,
	SYS_ipc_recv
};

#endif /* !YUOS_INC_SYSCALL_H */