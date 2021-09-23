#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "def.h"

int
main(void)
{
	Info *info;
	char path[512], *ref, *page, *sl;

	if(chdir("..") == -1)
		err(1, "chdir ..");
	if(unveil("data", "r") == -1
	|| unveil("html", "r") == -1)
		err(1, "unveil html");
	if(pledge("stdio rpath", nil) == -1)
		err(1, "pledge stdio rpath");

	info = cgiget(nil);

	if((ref = infostr(info, "ref")) == nil)
		err(1, "no $ref");
	if((page = infostr(info, "page")) == nil)
		err(1, "no $page");

	if((info = inforead(info, "data/info")) == nil)
		err(1, "parsing %s", path);
	for(sl = ref; (sl = strchr(sl, '/')); sl++){
		*sl = '\0';
		snprintf(path, sizeof path, "%s/info", ref);
		if((info = inforead(info, path)) == nil)
			err(1, "parsing %s", path);
		*sl = '/';
	}
	snprintf(path, sizeof path, "%s/info", ref);
	if((info = inforead(info, path)) == nil)
		err(1, "parsing %s", path);

	snprintf(path, sizeof path, "html/page.%s.html", page);

	cgihead();
	htmltemplate("html/head.html", info);
	htmltemplate(path, info);
	htmltemplate("html/foot.html", info);

	infofree(info);
	return 0;
}
