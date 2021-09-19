#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
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

static void
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
sel(struct dirent const *de)
{
	return de->d_name[0] != '.';
}

void
htmlcategories(void)
{
	struct dirent **dlist, **dp;
	int n;

	if((n = scandir("data", &dlist, sel, NULL)) == -1)
		err(1, "reading data directory");

	for(dp = dlist; n > 0; n--, dp++){
		fputs("<p>", stdout);
		htmlprint((*dp)->d_name);
		fputs("</p>\n", stdout);
		free(*dp);
	}
	free(dlist);
}
