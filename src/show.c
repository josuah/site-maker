#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "def.h"

int
main(void)
{
	Info *info;
	char path[256], *ref, *html, *sl;

	if(chdir("..") == -1)
		err(1, "chdir ..");
	if(unveil("data", "r") == -1
	|| unveil("html", "r") == -1)
		err(1, "unveil html");
	if(pledge("stdio rpath", nil) == -1)
		err(1, "pledge stdio rpath");

	info = cgiget(nil);

	if((ref = infostr(info, "ref")) == nil)
		cgierror(400, "no $ref");
	if((html = infostr(info, "html")) == nil)
		cgierror(400, "no $html");

	if((info = inforead(info, "data/info")) == nil)
		cgierror(500, "parsing %s: %s", "data/info", infoerr);
	for(sl = ref; (sl = strchr(sl, '/')); sl++){
		*sl = '\0';
		snprintf(path, sizeof path, "%s/info", ref);
		if((info = inforead(info, path)) == nil)
			cgierror(500, "parsing %s: %s", path, infoerr);
		*sl = '/';
	}
	snprintf(path, sizeof path, "%s/info", ref);
	if((info = inforead(info, path)) == nil)
		cgierror(500, "parsing %s: %s", path, infoerr);

	snprintf(path, sizeof path, "html/page.%s.html", html);

	cgihead();
	htmltemplate("html/head.html", info);
	htmltemplate(path, info);
	htmltemplate("html/foot.html", info);

	infofree(info);
	return 0;
}
