#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "dat.h"
#include "fns.h"

void
htmlprint(char *s)
{
	for(; *s != '\0'; s++){
		switch(*s){
		case '<':
			fputs("&lt;", stdout);
			break;
		case '>':
			fputs("&gt;", stdout);
			break;
		case '"':
			fputs("&quot;", stdout);
			break;
		case '&':
			fputs("&amp;", stdout);
			break;
		default:
			fputc(*s, stdout);
		}
	}
}

void
htmlcat(char *path)
{
	FILE *fp;
	char c;

	if((fp = fopen(path, "r")) == NULL)
		err(1, "opening %s", path);
	while((c = fgetc(fp)) != EOF)
		fputc(c, stdout);
	if(ferror(fp))
		err(1, "dumping %s", path);
	fclose(fp);
}

/*
 * extract parameters from head and bump tail pointer:
 *	text with {{parameter}} in some line
 *	          0 ^return  0 ^tail
 */
static char *
next(char *head, char **tail)
{
	char *beg, *end;

	if((beg = strstr(head, "{{")) == NULL
	|| (end = strstr(beg, "}}")) == NULL)
		return NULL;
	*beg = *end = '\0';
	*tail = end + strlen("}}");
	return beg + strlen("{{");
}

/*
 * takes a list of (Info *) arguments by order of precedence
 */
void
htmltemplate(char *htmlpath, ...)
{
	va_list va;
	Info *info;
	FILE *fp = NULL;
	size_t sz = 0;
	char *line = NULL, *head, *tail, *param, *val;

	if((fp = fopen(htmlpath, "r")) == NULL)
		err(1, "opening template %s", htmlpath);

	while(getline(&line, &sz, fp) > 0){
		head = tail = line;
		while((param = next(head, &tail)) != NULL){
			fputs(head, stdout);

			va_start(va, htmlpath);
			while((info = va_arg(va, Info *)) != NULL)
				if((val = infoget(info, param)) != NULL)
					break;
			va_end(va);

			if(val == NULL && (val = cgiquery(param)) == NULL)
				fprintf(stdout, "{{error:%s}}", param);
			else
				htmlprint(val);

			head = tail;
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}
