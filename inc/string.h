#ifndef YUOS_INC_STRING_H
#define YUOS_INC_STRING_H

#include <inc/types.h>

int strlen(const char *s);
int strnlen(const char *s, size_t size);
char * strchr(const char *s, char c);
int strcmp(const char *s1, const char *s2);
char * strfind(const char *s, char c);

void * memmove(void *dst, const void *src, size_t len);
void * memset(void *dst, int c, size_t len);

#endif /* not YUOS_INC_STRING_H */