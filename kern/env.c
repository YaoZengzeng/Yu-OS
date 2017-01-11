#include <kern/env.h>

struct Env *envs = NULL;		// All environments
struct Env *curenv	= NULL;		// The current env
static struct Env *env_free_list;	// Free environment list