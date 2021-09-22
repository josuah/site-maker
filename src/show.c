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
	char path[512], *page;

	if(chdir("..") == -1)
		err(1, "chdir ..");
	if(unveil("data", "r") == -1)
		err(1, "unveil data");
	if(unveil("html", "r") == -1)
		err(1, "unveil html");
	if(pledge("stdio rpath", nil) == -1)
		err(1, "pledge stdio rpath");

	cgihead();

	if((info = infodefault()) == NULL)
		err(1, "reading info files");

	if((page = cgiquery("page")) == NULL)
		err(1, "getting 'page' parameter: %s", cgierror);
	if(strchr(page, '/') != nil || page[0] == '.')
		errx(1, "invalid page name: %s", page);
	snprintf(path, sizeof path, "html/page.%s.html", page);

	htmltemplate("html/head.html", info);
	htmltemplate(path, info);
	htmltemplate("html/foot.html", info);

	infofree(info);

	return 0;
}
