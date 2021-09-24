#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "def.h"

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
		cgierror(500, "reading POST data");

	if((env = getenv("CONTENT_TYPE")) == nil)
		cgierror(500, "no $Content-Type");

	if(strcasecmp(env, "application/x-www-form-urlencoded") != 0)
		cgierror(400, "expecting application/x-www-form-urlencoded");

	info = cgiinfo(next, buf);
	info->buf = buf;
	return info;
}

void
cgifile(char *path, size_t len)
{
	FILE *fp;
	pid_t pid;
	char *env, *s, *bound, *line = nil;
	size_t sz = 0, boundlen;
	int c, nl;

	pid = getpid();

	snprintf(path, len, "tmp/%i", pid);
	if(mkdir(path, 0770) == -1)
		cgierror(500, "making temporary directory %s", path);

	snprintf(path, len, "tmp/%i/file", pid);
	if((fp = fopen(path, "w")) == nil)
		cgierror(500, "opening temporary upload file %s", path);

	if((env = getenv("CONTENT_TYPE")) == nil)
		cgierror(500, "no $Content-Type");

	s = "multipart/form-data; boundary=";
	if(strncasecmp(env, s, strlen(s)) != 0)
		cgierror(400, "expecting \"%s\"", s);
	bound = env + strlen(s);

	boundlen = strlen(bound);
	getline(&line, &sz, stdin);
	s = line;

	if(strncmp(s, "--", 2) != 0)
		cgierror(400, "cannot parse form data: %s", line);
	s += 2;

	if(strncmp(s, bound, boundlen) != 0)
		cgierror(400, "cannot recognise boundary");
	s += boundlen;
	s += (*s == '\r');

	if(*s != '\n')
		cgierror(400, "expecting newline after boundary");
	s++;

	nl = 0;
	while(getline(&line, &sz, stdin) > 0 && line[*line == '\r'] != '\n');

	if(feof(stdin))
		cgierror(400, "unexpected end-of-file before boundary end");

	while((c = fgetc(stdin)) != EOF)
		fputc(c, fp);

	fflush(fp);
	if(ferror(stdin) || ferror(fp))
		cgierror(500, "writing file to %s", path);

	ftruncate(fileno(fp), ftell(fp) - boundlen - strlen("----"));
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
	fprintf(stdout, "Status: %i %s\n", code, msg);
	fprintf(stdout, "Content-Type: text/plain\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Error %i: %s\n", code, msg);
	exit(0);
}

void
cgiredir(int code, char *fmt, ...)
{
	va_list va;
	char url[1024];

	va_start(va, fmt);
        vsnprintf(url, sizeof url, fmt, va);
	va_end(va);
	fprintf(stdout, "Status: %i redirecting\n", code);
	fprintf(stdout, "Location: %s\n", url);
	fprintf(stdout, "Content-Type: text/plain\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "redirecting to %s...\n", url);
}
