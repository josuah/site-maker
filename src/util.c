#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *arg0;

static void
_log(char const *fmt, va_list va, int err)
{
	if (arg0 != NULL)
		fprintf(stderr, "%s: ", arg0);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, err ? ": %s\n" : "\n", strerror(err));
	fflush(stderr);
}

void
err(int e, char const *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	_log( fmt, va, errno);
	exit(e);
}

void
errx(int e, char const *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	_log( fmt, va, 0);
	exit(e);
}

void
warn(char const *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	_log(fmt, va, errno);
}

void
warnx(char const *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	_log(fmt, va, 0);
}
