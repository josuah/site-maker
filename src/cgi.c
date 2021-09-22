#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dat.h"
#include "fns.h"

Info *
cgiinfo(char *s)
{
	Info *info;
	char *var, *eq;

	if((info = calloc(sizeof *info, 1)) == nil)
		err(1, "calloc");

	while((var = strsep(&s, "&"))){
		if((eq = strchr(var, '=')) == nil)
			continue;
		*eq = '\0';
		infoadd(info, var, eq + 1);
	}
	infosort(info);
	return info;
}

Info *
cgiget(void)
{
	static Info *info = nil;
	char *query;

	if(info)
		return info;
	if((query = getenv("QUERY_STRING")) == nil)
		err(1, "no $QUERY_STRING");
	return info = cgiinfo(query);
}

Info *
cgipost(void)
{
	return nil;
}

void
cgihead(void)
{
	fputs("Content-Type: text/html\n\n", stdout);
}

void
cgierror(int code, char *fmt, ...)
{
	va_list va;
	char msg[1024];

	va_start(va, fmt);
        vsnprintf(msg, sizeof msg, fmt, va);
	va_end(va);
	fprintf(stdout, "Status: %d %s\n", code, msg);
	fprintf(stdout, "Content-Type: text/plain\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Error %d: %s\n", code, msg);
	exit(0);
}

void
cgiredir(int code, char *url)
{
	fprintf(stdout, "Status: %d redirecting\n", code);
	fprintf(stdout, "Location: %s\n", url);
	fprintf(stdout, "Content-Type: text/plain\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "redirecting to %s...\n", url);
	exit(0);
}
