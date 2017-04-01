/*
 * Netwrok server main loop
 * serves IPC requests from other environments.
 */

#include <inc/x86.h>
#include <inc/string.h>
#include <inc/env.h>
#include <inc/ns.h>
#include <inc/lib.h>

#include "ns.h"

/* errno to make lwIP happy */
int errno;

#define debug 0

static envid_t input_envid;
static envid_t output_envid;

void
umain(int argc, char **argv)
{
	envid_t	ns_envid = sys_getenvid();

	binaryname = "ns";

	// fork off the input thread which will poll the NIC driver for input
	// packets
	input_envid = fork();
	if (input_envid < 0) {
		panic("error forking input environment");
	} else if (input_envid == 0) {
		input(ns_envid);
		return;
	}

	// fork off the output thread that will send the packets to the NIC
	// driver
	output_envid = fork();
	if (output_envid < 0) {
		panic("error forking output environment");
	} else if (output_envid == 0) {
		output(ns_envid);
		return;
	}
}