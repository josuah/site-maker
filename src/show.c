#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "def.h"

static void
show(void)
{
	Info *info;
	char path[256], *ref, *html, *sl;

	info = cgicookies(NULL);
	info = cgiget(info);

	if((html = infostr(info, "html")) == NULL)
		cgifatal("no $html");
	if((info = inforead(info, "data/info")) == NULL)
		cgifatal("parsing %s: %s", "data/info", infoerr);
	if((ref = infostr(info, "ref"))){
		for(sl = ref; sl && (sl = strchr(sl, '/')); sl++){
			*sl = '\0';
			snprintf(path, sizeof path, "%s/info", ref);
			if((info = inforead(info, path)) == NULL)
				cgifatal("parsing %s: %s", path, infoerr);
			*sl = '/';
		}
		snprintf(path, sizeof path, "%s/info", ref);
		if((info = inforead(info, path)) == NULL)
			cgifatal("parsing %s: %s", path, infoerr);
	}
	snprintf(path, sizeof path, "html/page.%s.html", html);
	cgihead("text/html");
	htmltemplate("html/head.html", info);
	htmltemplate(path, info);
	htmltemplate("html/foot.html", info);
	infofree(info);
}

int
main(int argc, char **argv)
{
	(void)argc;
	argv0 = argv[0];

	if(chdir("..") == -1)
		sysfatal("chdir ..");
	if(unveil("data", "r") == -1
	|| unveil("news", "r") == -1
	|| unveil("html", "r") == -1)
		sysfatal("unveil html");
	if(pledge("stdio rpath", NULL) == -1)
		sysfatal("pledge stdio rpath");
	show();
	return 0;
}
