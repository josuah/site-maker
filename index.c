#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libhttpd.h"

static void
show_404(int method, char const *path, char const *matches[])
{
	(void)method;
	(void)path;
	(void)matches;

	httpd_send_headers(404, "text/html");
	printf("<h1>404: page not found</h1>\n");
}

static void
show_test(int method, char const *path, char const *matches[])
{
	(void)method;
	(void)path;
	(void)matches;

	httpd_send_headers(200, "text/html");
	printf("<h1>You found the test page</h1>\n");
}

static struct httpd_handler handlers[] = {
	{ "/test",	show_test },
	{ "/*",		show_404 },
	{ NULL,		NULL },
};

int
main(void)
{
	pledge("stdio", NULL);
	httpd_handle_request(handlers);
	return 0;
}
