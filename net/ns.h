#include <inc/ns.h>
#include <inc/x86.h>
#include <inc/lib.h>

#define IP "10.0.2.15"
#define MASK "255.255.255.0"
#define DEFAULT "10.0.2.2"

#define TIMER_INTERVAL 250

// Virtual address at which to receive page mappings containing client requests.
#define QUEUE_SIZE	20
#define REQVA	(0x0ffff000 - QUEUE_SIZE * PGSIZE)

/* input.c */
void input(envid_t ns_envid);

/* output.c */
void output(envid_t ns_envid);
