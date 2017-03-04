#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running, Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	int i, start;
	i = curenv == NULL ? 0 : (ENVX(curenv->env_id) + 1) % NENV;
	start = curenv == NULL ? NENV - 1 : ENVX(curenv->env_id);

	for (; i != start; i = (i + 1) % NENV) {
		if (envs[i].env_status == ENV_RUNNABLE) {
			env_run(&envs[i]);
		}
	}

	if (envs[start].env_status == ENV_RUNNABLE || envs[start].env_status == ENV_RUNNING) {
		env_run(&envs[start]);
	}

	monitor(NULL);
}
