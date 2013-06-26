#include <unistd.h>
#include <stdio.h>

int
main(void)
{
	if (isatty(STDIN_FILENO)) {
		return 0;
	}
	fputs("stdin is not a tty\n", stderr);
	return 1;
}
