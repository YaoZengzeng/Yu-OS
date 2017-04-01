#ifndef YUOS_INC_NS_H
#define YUOS_INC_NS_H

#include <inc/types.h>
#include <inc/mmu.h>

struct jif_pkt {
	int jp_len;
	char jp_data[0];
};

// Definition for requests from clients to network server
enum {
	// The following messages pass a page containing an Nsipc.

	// The following two messages pass a page containing a struct jif_pkt
	NSREQ_INPUT,
	// NSREQ_OUTPUT, unlike all other messages, is sent *from* the
	// network server, to the output environment
	NSREQ_OUTPUT,
};

union Nsipc {
	struct jif_pkt pkt;

	// Ensure Nsipc is one page
	char _pad[PGSIZE];
};

#endif /* !YUOS_INC_NS_H */