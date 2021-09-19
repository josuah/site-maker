#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
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

static int
iscat(struct dirent const *de)
{
	return strncmp(de->d_name, "cat", 3) == 0;
}

void
htmlcategories(void)
{
	struct dirent **dlist, **dp;
	int n;

	if((n = scandir("data", &dlist, iscat, NULL)) == -1)
		err(1, "reading data directory");

	for(dp = dlist; n > 0; n--, dp++){
		fputs("<a href=\"/cgi/page-category?cat=", stdout);
		htmlprint((*dp)->d_name);
		fputs("</p>\n", stdout);
		free(*dp);
	}
	free(dlist);
}
