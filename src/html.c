#include <dirent.h>
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

static int
iscat(struct dirent const *de)
{
	return strncmp(de->d_name, "cat", 3) == 0;
}

static int
isitem(struct dirent const *de)
{
	return strncmp(de->d_name, "item", 4) == 0;
}

static int
isimg(struct dirent const *de)
{
	return strncmp(de->d_name, "img", 3) == 0;
}

void
htmllist(Info *info, char *name, char *fmt, int (*fn)(struct dirent const *))
{
	struct dirent **dlist, **dp;
	char *d, path[128];
	char const *errstr;
	size_t cat, item;
	int n;

	cat = cgiquerynum("cat", 1, CAT_MAX, &errstr);
	item = cgiquerynum("cat", 1, CAT_MAX, &errstr);

	snprintf(path, sizeof path, fmt, cat, item);
	if((n = scandir(path, &dlist, fn, nil)) == -1)
		err(1, "reading data directory");
	for(dp = dlist; n > 0; n--, dp++){
		d = (*dp)->d_name;

		if(infoset(info, "elem", d + strcspn(d, "0123456789")) == -1)
			err(1, "infoset");

		snprintf(path, sizeof path, "html/elem.%s.html", name);
		htmltemplate(path, info);

		free(*dp);
	}

	free(dlist);
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

	if((beg = strstr(head, "{{")) == nil
	|| (end = strstr(beg, "}}")) == nil)
		return nil;
	*beg = *end = '\0';
	*tail = end + strlen("}}");
	return beg + strlen("{{");
}

/*
 * takes a list of (Info *) arguments by order of precedence
 */
void
htmltemplate(char *htmlpath, Info *info)
{
	FILE *fp = nil;
	size_t sz = 0;
	char *line = nil, *head, *tail, *param, *val;

	if((fp = fopen(htmlpath, "r")) == nil)
		err(1, "opening template %s", htmlpath);

	while(getline(&line, &sz, fp) > 0){
		head = tail = line;
		for(; (param = next(head, &tail)) != nil; head = tail){
			fputs(head, stdout);

			if(strcmp(param, "list:cat") == 0){
				htmllist(info, "cat", "data", iscat);
				continue;
			}
			if(strcmp(param, "list:item") == 0){
				htmllist(info, "item", "data/cat%zu", isitem);
				continue;
			}
			if(strcmp(param, "list:img") == 0){
				htmllist(info, "img", "data/cat%zu/item%zu", isimg);
				continue;
			}

			if((val = infoget(info, param)) == nil)
				fprintf(stdout, "{{error:%s}}", param);
			else
				htmlprint(val);
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}
