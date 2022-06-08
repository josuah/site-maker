#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libhttpd.h"

static void
show_404(int method, char **matches)
{
	(void)method;
	(void)matches;

	httpd_send_headers(404, "text/html");
	printf("<h1>404: page '%s' not found</h1>\n", matches[0]);
}

static void
show_test(int method, char **matches)
{
	(void)method;
	(void)matches;

	httpd_send_headers(200, "text/html");
	printf("<h1>You found the test page</h1>\n");
}

static void
show_home(int method, char **matches)
{
	struct httpd_var_list vars = {0};
	(void)method;
	(void)matches;

	httpd_set_var(&vars, "title", "Accueil - metairies.org");
	httpd_send_headers(200, "text/html");
	httpd_template("html/head.html", &vars);
	httpd_template("html/foot.html", &vars);
}

static struct httpd_handler handlers[] = {
	{ "/test/",	show_test },
	{ "/",		show_home },
	{ "*",		show_404 },
	{ NULL,		NULL },
};

int
main(void)
{
	unveil("html", "r");
	pledge("stdio rpath", NULL);
	httpd_handle_request(handlers);
	return 0;
}
