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

Info headers[1], cookies[1];

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

static Info *
parsequery(Info *next, char *s)
{
	Info *info;
	char *var, *eq;

	if((info = calloc(sizeof *info, 1)) == nil)
		sysfatal("calloc");

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
		cgifatal("no $QUERY_STRING");
	return parsequery(next, query);
}

Info *
cgipost(Info *next)
{
	Info *info;
	size_t len;
	char *buf, *env;

	if((env = getenv("CONTENT_LENGTH")) == nil)
		sysfatal("no $CONTENT_LENGTH");
	len = atoi(env);

	if((buf = calloc(len + 1, 1)) == nil)
		sysfatal("calloc");

	fread(buf, len, 1, stdin);
	if(ferror(stdin) || feof(stdin))
		cgifatal("reading POST data");

	if((env = getenv("CONTENT_TYPE")) == nil)
		cgifatal("no $Content-Type");

	if(strcasecmp(env, "application/x-www-form-urlencoded") != 0)
		cgifatal("expecting application/x-www-form-urlencoded");

	info = parsequery(next, buf);
	info->buf = buf;
	return info;
}

Info *
cgicookies(Info *next)
{
	Info *info;
	char *env, *val;

	if((info = calloc(sizeof *info, 1)) == nil)
		sysfatal("calloc");
	if((env = getenv("HTTP_COOKIE")) == nil)
		return info;
	while((val = strsep(&env, ";"))){
		val += (*val == ' ');
		infoadd(info, strsep(&val, "="), val);
	}
	infosort(info);
	info->next = next;
	return info;
}

void
cgifile(char *path, size_t len)
{
	FILE *fp;
	pid_t pid;
	char *env, *s, *bound, *line;
	size_t sz, boundlen;
	int c, nl;

	pid = getpid();
	line = nil;
	sz = 0;

	snprintf(path, len, "tmp/%i", pid);
	if(mkdir(path, 0770) == -1)
		cgifatal("making temporary directory %s", path);

	snprintf(path, len, "tmp/%i/file", pid);
	if((fp = fopen(path, "w")) == nil)
		cgifatal("opening temporary upload file %s", path);

	if((env = getenv("CONTENT_TYPE")) == nil)
		cgifatal("no $Content-Type");

	s = "multipart/form-data; boundary=";
	if(strncasecmp(env, s, strlen(s)) != 0)
		cgifatal("expecting \"%s\"", s);
	bound = env + strlen(s);

	boundlen = strlen(bound);
	getline(&line, &sz, stdin);
	s = line;

	if(strncmp(s, "--", 2) != 0)
		cgifatal("cannot parse form data: %s", line);
	s += 2;

	if(strncmp(s, bound, boundlen) != 0)
		cgifatal("cannot recognise boundary");
	s += boundlen;
	s += (*s == '\r');

	if(*s != '\n')
		cgifatal("expecting newline after boundary");
	s++;

	nl = 0;
	while(getline(&line, &sz, stdin) > 0 && line[*line == '\r'] != '\n');

	if(feof(stdin))
		cgifatal("unexpected end-of-file before boundary end");

	while((c = fgetc(stdin)) != EOF)
		fputc(c, fp);

	fflush(fp);
	if(ferror(stdin) || ferror(fp))
		cgifatal("writing file to %s", path);

	ftruncate(fileno(fp), ftell(fp) - boundlen - strlen("----"));
}

void
cgihead(char *type)
{
	InfoRow *row;
	size_t n;

	fprintf(stdout, "Content-Type: %s\n", type);

	for(row = headers->vars, n = headers->len; n > 0; n--, row++)
		if(strcasecmp(row->key, "text") != 0)
			fprintf(stdout, "%s: %s\n", row->key, row->val);

	for(row = cookies->vars, n = cookies->len; n > 0; n--, row++)
		fprintf(stdout, "Set-Cookie: %s=%s\n", row->key, row->val);

	fputc('\n', stdout);
}

void
cgifatal(char *fmt, ...)
{
	va_list va;
	char msg[1024];

	va_start(va, fmt);
        vsnprintf(msg, sizeof msg, fmt, va);
	va_end(va);
	fprintf(stdout, "Status: 500 %s\n", msg);
	fprintf(stdout, "Content-Type: text/plain\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Error: %s\n", msg);
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
	cgihead("text/plain");
	fprintf(stdout, "Redirecting to %s\n", url);
}
