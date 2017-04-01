#ifndef YUOS_INC_NETE1000_H
#define YUOS_INC_NETE1000_H

#include <inc/types.h>

struct tx_desc {
	uint64_t	addr;
	uint16_t	length;
	uint8_t		cso;
	uint8_t		cmd;
	uint8_t		status;
	uint8_t		css;
	uint16_t	special;
};

struct rx_desc {
	uint64_t	addr;
	uint16_t	length;
	uint16_t	checksum;
	uint8_t		status;
	uint8_t		errors;
	uint16_t	special;
};

#endif