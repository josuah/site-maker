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
	Info *in0, *in1, *in2;
	int n;
	size_t cat;
	char path[128], *d;
	char const *errstr;

	if(unveil("html", "r") == -1)
		err(1, "unveil html");
	if(unveil("data", "r") == -1)
		err(1, "unveil data");
	if(pledge("stdio rpath", NULL) == -1)
		err(1, "pledge stdio rpath");

	cgihead();

	if((in0 = infonew("data/info")) == NULL)
		err(1, "parsing data/info");

	cat = cgiquerynum("cat", 1, CAT_MAX, &errstr);
	if(errstr)
		err(1, "getting query var 'cat': %s", cgierror);

	snprintf(path, sizeof path, "data/cat%zu", cat);
	if((n = scandir(path, &dlist, isitem, NULL)) == -1)
		err(1, "reading %s/", path);

	snprintf(path, sizeof path, "data/cat%zu/info", cat);
	if((in1 = infonew(path)) == NULL)
		err(1, "parsing %s", path); 

	htmltemplate("html/all.head.html", in1, in0, NULL);
	htmltemplate("html/category.head.html", in1, in0, NULL);

	for(dp = dlist; n > 0; n--, dp++){
		d = (*dp)->d_name;

		snprintf(path, sizeof path, "data/cat%zu/%s/info", cat, d);
		if((in2 = infonew(path)) == NULL)
			err(1, "parsing %s", path); 

		if(infoset(in2, "item", d + strcspn(d, "0123456789")) == -1)
			err(1, "infoset");

		htmltemplate("html/category.item.html", in2, in1, in0, NULL);

		infofree(in2);
		free(*dp);
	}

	free(dlist);
	infofree(in1);
	infofree(in0);

	htmltemplate("html/category.foot.html", in1, in0, NULL);
	htmltemplate("html/all.foot.html", in1, in0, NULL);

	return 0;
}
