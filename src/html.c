#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
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

void
htmltemplate(char *htmlpath, Info *info)
{
	size_t sz = 0;
	FILE *fp = NULL;
	char *line = NULL, *head, *tail, *param, *val;

	if((fp = fopen(htmlpath, "r")) == NULL)
		err(1, "opening template %s", htmlpath);

	while(getline(&line, &sz, fp) > 0){
		head = tail = line;
		while((param = next(head, &tail)) != NULL){
			fputs(head, stdout);
			if((val = infoget(info, param)) == NULL
			&& (val = cgiquery(param)) == NULL)
				fprintf(stdout, "{{error:%s}}", param);
			else
				htmlprint(val);
			head = tail;
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}
