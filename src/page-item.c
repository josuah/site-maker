#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "dat.h"
#include "fns.h"

static int
isimg(struct dirent const *de)
{
	return strncmp(de->d_name, "img", 3) == 0;
}

int
main(void)
{
	struct dirent **dlist, **dp;
	Info *in0, *in1, *in2;
	int n;
	size_t cat, item;
	char path[128], *d;
	char const *errstr;

	if(unveil("html", "r") == -1)
		err(1, "unveil html");
	if(unveil("data", "r") == -1)
		err(1, "unveil data");
	if(pledge("stdio rpath", NULL) == -1)
		err(1, "pledge stdio rpath");

	cgihead();

	cat = cgiquerynum("cat", 1, CAT_MAX, &errstr);
	if(errstr)
		err(1, "getting query var 'cat': %s", cgierror);

	item = cgiquerynum("item", 1, CAT_MAX, &errstr);
	if(errstr)
		err(1, "getting query var 'item': %s", cgierror);

	if((in0 = infonew("data/info")) == NULL)
		err(1, "parsing data/info");

	snprintf(path, sizeof path, "data/cat%zu/info", cat);
	if((in1 = infonew(path)) == NULL)
		err(1, "parsing %s", path); 

	snprintf(path, sizeof path, "data/cat%zu/item%zu/info", cat, item);
	if((in2 = infonew(path)) == NULL)
		err(1, "parsing %s", path); 

	htmltemplate("html/all.head.html", in2, in1, in0, NULL);
	htmltemplate("html/item.head.html", in2, in1, in0, NULL);

	snprintf(path, sizeof path, "data/cat%zu/item%zu", cat, item);
	if((n = scandir(path, &dlist, isimg, NULL)) == -1)
		err(1, "reading data directory");

	for(dp = dlist; n > 0; n--, dp++){
		d = (*dp)->d_name;

		if(infoset(in2, "img", d) == -1)
			err(1, "infoset");

		htmltemplate("html/item.img.html", in2, in1, in0, NULL);

		free(*dp);
	}

	free(dlist);

	htmltemplate("html/item.foot.html", in2, in1, in0, NULL);
	htmltemplate("html/all.foot.html", in2, in1, in0, NULL);

	infofree(in2);
	infofree(in1);
	infofree(in0);

	return 0;
}
