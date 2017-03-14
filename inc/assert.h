#ifndef YUOS_INC_ASSERT_H
#define YUOS_INC_ASSERT_H

#include <inc/stdio.h>

void _panic(const char*, int, const char*, ...) __attribute__((noreturn));

#define panic(...) _panic(__FILE__, __LINE__, __VA_ARGS__)

#define assert(x)	\
	do { if (!(x)) panic("assertion failed: %s", #x); } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)	switch (x) case 0: case (x):

#endif /* !YUOS_INC_ASSERT_H */