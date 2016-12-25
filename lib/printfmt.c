// Stripped-down primitive printf-style formatting routines,
// used in common by printf, sprintf, fprintf, etc.
// This code is also used by both the kernel and user programs.

#include <inc/types.h>
#include <inc/error.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>

/*
 * Space or zero padding and a field width are supported for the numeric
 * formats only.
 *
 * The special format %e takes an integer error code
 * and prints a string describing the error.
 * THe integer may be positive or negative,
 * so that -E_NO_MEM and E_NO_MEM are equivalent.
 */

 static const char * const error_string[MAXERROR] =
 {
 	[E_UNSPECIFIED] = "unspecified error",
 	[E_BAD_ENV] = "bad environment",
 	[E_INVAL] = "invalid parameter",
 	[E_NO_MEM] = "out of memory",
 	[E_NO_FREE_ENV] = "out of environments",
 	[E_FAULT] = "segmentation fault",
 };

void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap)
{
	register const char *p;
	register int ch, err;
	unsigned long long num;
	int base, lflag, width, precision, altflag;
	char padc;

	while (1) {
		while ((ch = *(unsigned char *) fmt++) != '%') {
			if (ch == '\0') {
				return;
			}
			putch(ch, putdat);
		}
/*
		// Process a %-escape sequence
		padc = ' ';
		width = -1;
		precision = -1;
		lflag = 0;
		altflag = 0;
	reswitch:
		switch (ch = *(unsigned char *) fmt++) {

		// flag to pad on the right
		case '-':
			padc = '-';
			goto reswitch;

		// flag to pad with 0's instead of spaces
		case '0':
			padc = '0';
			goto reswitch;

		// width field
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			for (precision = 0; ; ++fmt) {
				precision = precision * 10 + ch -'0';
				ch = *fmt;
				if (ch < '0' || ch > '9') {
					break;
				}
			}
			goto process_precision;

		case '*':
			precision = va_arg(ap, int);
			goto process_precision;

		case '.':
			if (width < 0) {
				width = 0;
			}
			goto reswitch;

		case '#':
			altflag = 1;
			goto reswitch;

		process_precision:
			if (width < 0) {
				width = precision, precision = -1;
			}
			goto reswitch;

		// long flag (double for long long)
		case 'l':
			lflag++;
			goto reswitch;

		// character
		case 'c':
			putch(va_arg(ap, int), putdat);
			break;

		// error message
		case 'e':
			err = va_arg(ap, int);
			if (err < 0) {
				err = -err;
			}
			if (err >= MAXERROR || (p = error_string[err]) == NULL) {
				printfmt(putch, putdat, "error %d", err);
			} else {
				printfmt(putch, putdat, "%s", p);
			}
			break;

		// (signed) decimal
		case 'd':
			num = getint(&ap, lflag);
			if ((long long) num < 0) {
				putch('-', putdat);
				num = -(long long) num;
			}
			base = 10;
			goto number;

		// unsigned decimal
		case 'u':
			num = getuint(&ap, lflag);
			base = 10;
			goto number;

		// (unsigned) octal
		case 'o':
			num = getuint(&ap, lflag);
			base = 8;
			goto number;

		// (unsigned) hexadecimal
		case 'x':
			num = getuint(&ap, lflag);
			base = 16;
		number:
			printnum(putch, putdat, num, base, width, padc);
			break;

		// escape '%' character
		case '%':
			putch(ch, putdat);
			break;

		}*/
	}
}