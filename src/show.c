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

	info = cgi_cookies(NULL);
	info = cgi_get(info);

	if ((html = info_str(info, "html")) == NULL)
		cgi_fatal("no $html");
	if ((info = info_read(info, "data/info")) == NULL)
		cgi_fatal("parsing %s: %s", "data/info", info_err);
	if ((ref = info_str(info, "ref"))) {
		for (sl = ref; sl && (sl = strchr(sl, '/')); sl++) {
			*sl = '\0';
			snprintf(path, sizeof path, "data/%s/info", ref);
			if ((info = info_read(info, path)) == NULL)
				cgi_fatal("parsing %s: %s", path, info_err);
			*sl = '/';
		}
		snprintf(path, sizeof path, "data/%s/info", ref);
		if ((info = info_read(info, path)) == NULL)
			cgi_fatal("parsing %s: %s", path, info_err);
	}
	snprintf(path, sizeof path, "html/page.%s.html", html);
	cgi_head("text/html");
	html_template("html/head.html", info);
	html_template(path, info);
	html_template("html/foot.html", info);
	info_free(info);
}

int
main(int argc, char **argv)
{
	(void)argc;
	argv0 = argv[0];

	if (chdir("..") == -1)
		sysfatal("chdir ..");
	if (unveil("data", "r") == -1
	|| unveil("html", "r") == -1)
		sysfatal("unveil html");
	if (pledge("stdio rpath", NULL) == -1)
		sysfatal("pledge stdio rpath");
	show();
	return 0;
}
