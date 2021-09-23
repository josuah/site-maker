#include <dirent.h>
#include <unistd.h>
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

static void
list(Info *info, char *name, char *fmt, int (*fn)(struct dirent const *), int infofile)
{
	Info tmp;
	InfoRow row;
	struct dirent **dlist, **dp;
	char *d, path[128];
	size_t cat, item;
	int n;

	cat = infonum(info, "cat", 1, 10000);
	item = infonum(info, "item", 1, 10000);

	snprintf(path, sizeof path, fmt, cat, item);
	if((n = scandir(path, &dlist, fn, nil)) == -1)
		err(1, "reading data directory");
	for(dp = dlist; n > 0; n--, dp++){
		d = (*dp)->d_name;

		if(infofile){
			snprintf(path, sizeof path, fmt, cat, item);
			snprintf(path, sizeof path, "%s/%s/info", path, d);
			if((info = inforead(info, path)) == nil)
				err(1, "parsing %s", path);
		}

		row.key = "elem";
		row.val = d + strcspn(d, "0123456789");
		tmp.vars = &row;
		tmp.next = info;
		tmp.len = 1;
		tmp.buf = nil;

		snprintf(path, sizeof path, "html/elem.%s.html", name);
		htmltemplate(path, &tmp);

		if(infofile)
			info = infopop(info);
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
				list(info, "cat", "data", iscat, 1);

			}else if(strcmp(param, "list:item") == 0){
				list(info, "item", "data/cat%zu", isitem, 1);

			}else if(strcmp(param, "list:img") == 0){
				list(info, "img", "data/cat%zu/item%zu", isimg, 0);

			}else if(strcmp(param, "list:admincat") == 0){
				list(info, "admincat", "data", iscat, 1);

			}else if(strcmp(param, "list:adminitem") == 0){
				list(info, "adminitem", "data/cat%zu", isitem, 1);

			}else if((val = infostr(info, param)) == nil){
				fprintf(stdout, "{{error:%s}}", param);

			}else{
				htmlprint(val);
			}
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}
