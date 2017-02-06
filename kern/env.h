#ifndef YUOS_KERN_ENV_H
#define	YUOS_KERN_ENV_H

#include <inc/env.h>

extern struct Env *envs;		// All environments
extern struct Env *curenv;		// Current environment

void 	env_init(void);
void 	env_init_percpu(void);

#endif 	/* !YUOS_KERN_ENV_H */