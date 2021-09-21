#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "dat.h"
#include "fns.h"

static int
isitem(struct dirent const *de)
{
	return strncmp(de->d_name, "item", 4) == 0;
}

int
main(void)
{
	struct dirent **dlist, **dp;
	Info *info;
	int n, i;
	char path1[256], path2[256], *d;
	char const *errstr;

	if(unveil("html", "r") == -1)
		err(1, "unveil html");
	if(unveil("data", "r") == -1)
		err(1, "unveil data");
	if(pledge("stdio rpath", NULL) == -1)
		err(1, "pledge stdio rpath");

	htmltemplate("html/all.head.html", NULL);
	htmltemplate("html/category.head.html", NULL);

	n = cgiquerynum("cat", 1, CAT_MAX, &errstr);
	if(errstr)
		err(1, "getting query var 'cat': %s", cgierror);
	snprintf(path1, sizeof path1, "data/cat%zu", n);

	if((n = scandir(path1, &dlist, isitem, NULL)) == -1)
		err(1, "reading data directory");
	for(dp = dlist; n > 0; n--, dp++){
		d = (*dp)->d_name;
		snprintf(path2, sizeof path2, "%s/%s/info", path1, d);
		if((info = infoopen(path2)) == NULL)
			err(1, "parsing %s", path2); 
		if(infoset(info, "item", d + strcspn(d, "0123456789")) == -1)
			err(1, "infoset");
		for(i = 0; i < info->len; i++){
			InfoRow *r = info->vars + i;
			warn("var%d=%s=%s", i, r->key, r->val);
		}
		htmltemplate("html/category.item.html", info);
		free(*dp);
	}
	free(dlist);
	infoclose(info);

	htmltemplate("html/category.foot.html", NULL);
	htmltemplate("html/all.foot.html", NULL);

	return 0;
}
