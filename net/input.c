#include "ns.h"
#include <inc/lib.h>

extern union Nsipc nsipcbuf;

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// read a packet from the device driver
	// send it to the network server.
	// Hint: When we IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int r;
	while(1) {
		if ((r = sys_page_alloc(0, &nsipcbuf, PTE_U | PTE_W | PTE_P)) != 0) {
			continue;
		}

		struct rx_desc rd = {0, 0, 0, 0, 0, 0};
		rd.addr = (uintptr_t)&nsipcbuf.pkt.jp_data;
		while(1) {
			if (sys_rx_pkt(&rd) == 0) {
				break;
			} else {
				sys_yield();
			}
		}
		nsipcbuf.pkt.jp_len = rd.length;

again:
		if ((r = sys_ipc_try_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P | PTE_W | PTE_U)) < 0) {
			if (r == -E_IPC_NOT_RECV) {
				sys_yield();
				goto again;
			}
			panic("input: sys_ipc_try_send failed\n");
		}
	}
}
