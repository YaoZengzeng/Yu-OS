// Called from entry.S to get us going.
// entry.S already took care of defining envs, pages, and uvpt.

#include <inc/lib.h>

extern void main(int argc, char **argv);

const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void
libmain(int argc, char **argv)
{
	// set thisenv to point at our Env structure in envs[].
	thisenv = 0;

	// save the name of the program so that panic() can use it
	if (argc > 0) {
		binaryname = argv[0];
	}

	// call user main routine
	umain(argc, argv);

	// exit gracefully
	//exit();
}