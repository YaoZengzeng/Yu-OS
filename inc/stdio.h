#ifndef YUOS_INC_STDIO_H
#define YUOS_INC_STDIO_H

#include <inc/stdarg.h>

#ifndef NULL
#define NULL 	((void *) 0)
#endif /* !NULL */

void cputchar(int c);
int getchar(void);
int iscons(int fd);

void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list);

int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list);

char* readline(const char *prompt);
#endif /* !YUOS_INC_STDIO_H */