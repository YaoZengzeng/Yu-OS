#ifndef YUOS_INC_STRING_H
#define YUOS_INC_STRING_H

#include <inc/types.h>

int strnlen(const char *s, size_t size);
void * memmove(void *dst, const void *src, size_t len);
void * memset(void *dst, int c, size_t len);

#endif /* not YUOS_INC_STRING_H */