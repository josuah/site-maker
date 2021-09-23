#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "def.h"

/* https://pubs.opengroup.org/onlinepubs/9699919799/functions/exec.html */
extern char **environ;

#define HEX(x)(\
	((x) >= 'a' && (x) <= 'f') ? 10 + (x) - 'a' :\
	((x) >= 'A' && (x) <= 'F') ? 10 + (x) - 'A' :\
	((x) >= '0' && (x) <= '9') ? (x) - '0' :\
	0\
)

static void
decode(char *s)
{
	char *w;

	for(w = s;;){
		switch(*s){
		case '+':
			*w++ = ' ';
			s++;
			break;
		case '%':
			if(!isxdigit(s[1]) || !isxdigit(s[2])){
				s++;
				continue;
			}
			*w++ = (HEX(s[1]) << 4) | HEX(s[2]);
			s += 3;
			break;
		case '\0':
			*w = '\0';
			return;
		default:
			*w++ = *s;
			s++;
		}
	}
}

Info *
cgiinfo(Info *next, char *s)
{
	Info *info;
	char *var, *eq;

	if((info = calloc(sizeof *info, 1)) == nil)
		err(1, "calloc");

	while((var = strsep(&s, "&"))){
		decode(var);
		if((eq = strchr(var, '=')) == nil)
			continue;
		*eq = '\0';
		infoadd(info, var, eq + 1);
	}
	infosort(info);
	info->next = next;
	return info;
}

Info *
cgiget(Info *next)
{
	char *query;

	if((query = getenv("QUERY_STRING")) == nil)
		errx(1, "no $QUERY_STRING");
	return cgiinfo(next, query);
}

Info *
cgienv(Info *next)
{
	static Info *info = nil;
	char **envp, *env, *var, *eq;
	size_t sz = 0, len;

	if((info = calloc(sizeof *info, 1)) == nil)
		err(1, "calloc");
	for(envp = environ; *envp; envp++){
		if(strncmp("HTTP_", *envp, strlen("HTTP_")) != 0)
			continue;

		env = *envp + strlen("HTTP_");
		var = info->buf + sz;
		len = strlen(env);

		if((info->buf = realloc(info->buf, sz += len + 1)) == nil)
			err(1, "realloc");
		memmove(var, env, len + 1);

		eq = strchr(var, '=');
		assert(eq);
		*eq = '\0';
		infoadd(info, tr(var, "_", "-"), eq + 1);
	}
	info->next = next;
	return info;
}

Info *
cgipost(Info *next)
{
	Info *info;
	size_t len;
	char *buf, *env;

	if((env = getenv("CONTENT_LENGTH")) == nil)
		errx(1, "no $CONTENT_LENGTH");
	len = atoi(env);

	if((buf = calloc(len + 1, 1)) == nil)
		err(1, "calloc");

	fread(buf, len, 1, stdin);
	if(ferror(stdin) || feof(stdin))
		err(1, "reading POST data");

	info = cgiinfo(next, buf);
	info->buf = buf;
	return info;
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
}
