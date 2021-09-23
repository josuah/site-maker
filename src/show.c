#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "dat.h"
#include "fns.h"

int
main(void)
{
	Info *info;
	size_t cat, item;
	char path[512], *page;

	if(chdir("..") == -1)
		err(1, "chdir ..");
	if(unveil("data", "r") == -1
	|| unveil("html", "r") == -1)
		err(1, "unveil html");
	if(pledge("stdio rpath", nil) == -1)
		err(1, "pledge stdio rpath");

	info = cgiget(nil);
	cat = infonum(info, "cat", 1, 100000);
	item = infonum(info, "item", 1, 100000);
	page = infostr(info, "page");

	if((info = inforead(info, "data/info")) == nil)
		err(1, "inforead %s", "data/info");
	if(cat == 0)
		goto Done;
	snprintf(path, sizeof path, "data/cat%zu/info", cat);
	if((info = inforead(info, path)) == nil)
		err(1, "inforead %s", path);
	if(item == 0)
		goto Done;
	snprintf(path, sizeof path, "data/cat%zu/item%zu/info", cat, item);
	if((info = inforead(info, path)) == nil)
		err(1, "inforead %s", path);
Done:
	if(page == nil)
		err(1, "no $page: %s", infoerr);
	if(strchr(page, '/') != nil || page[0] == '.')
		errx(1, "invalid page name: %s", page);
	snprintf(path, sizeof path, "html/page.%s.html", page);

	cgihead();
	htmltemplate("html/head.html", info);
	htmltemplate(path, info);
	htmltemplate("html/foot.html", info);

	infofree(info);
	return 0;
}
