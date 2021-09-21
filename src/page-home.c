#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "dat.h"
#include "fns.h"

static int
iscat(struct dirent const *de)
{
	return strncmp(de->d_name, "cat", 3) == 0;
}

int
main(void)
{
	Info *info;
	struct dirent **dlist, **dp;
	int n;
	char path[128], *d;

	if(unveil("html", "r") == -1 || unveil("data", "r") == -1)
		errx(1, "unveil");
	if(pledge("stdio rpath", NULL) == -1)
		errx(1, "pledge");

	htmltemplate("html/all.head.html", NULL);
	htmltemplate("html/home.head.html", NULL);

	if((n = scandir("data", &dlist, iscat, NULL)) == -1)
		err(1, "reading data directory");
	for(dp = dlist; n > 0; n--, dp++){
		d = (*dp)->d_name;
		snprintf(path, sizeof path, "data/%s/info", d);
		if((info = infoopen(path)) == NULL)
			err(1, "parsing %s", path);
		if(infoset(info, "cat", d + strcspn(d, "0123456789")) == -1)
			err(1, "preparing template infos");
		htmltemplate("html/home.category.html", info);
		free(*dp);
	}
	free(dlist);

	htmltemplate("html/home.foot.html", NULL);
	htmltemplate("html/all.foot.html", NULL);

	return 0;
}
