#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libhttpd.h"

struct httpd_var_list vars;

static void
init_website(void)
{
	char *categories;

	httpd_read_var_list(&vars, "db/website");

	httpd_send_headers(404, "text/html");
	httpd_template("html/head.html", &vars);

	printf("<nav>\n");
	categories = httpd_get_var(&vars, "categories");
	for (char *s = categories; (s = strsep(&categories, " ")) != NULL;) {
		struct httpd_var_list category = {0};
		char path[1024];

		snprintf(path, sizeof path, "db/cat/%s", s);
		httpd_read_var_list(&category, path);
		httpd_template("html/elem-nav-category.html", &category);
	}
	printf("<a href=\"/cart/\">Panier</a>\n");
	printf("</nav>\n");
	printf("<main>\n");

}

static void
fini_website(void)
{
	printf("</main>\n");
}

static void
error_404(char **matches)
{
	(void)matches;

	init_website();
	httpd_template("html/404.html", &vars);
	fini_website();
}

static void
get_home(char **matches)
{
	(void)matches;

	init_website();
	httpd_template("html/page-home.html", &vars);
	fini_website();
}

static void
get_category(char **matches)
{
	(void)matches;

	init_website();
	httpd_template("html/page-category.html", &vars);
	fini_website();
}

static void
get_item(char **matches)
{
	(void)matches;

	init_website();
	httpd_template("html/page-item.html", &vars);
	fini_website();
}

static struct httpd_handler handlers[] = {
	{ HTTPD_GET,	"/",			get_home },
	{ HTTPD_GET,	"/category/*/",		get_category },
	{ HTTPD_GET,	"/item/*/",		get_item },
	{ HTTPD_GET,	"/cart/*/",		get_item },
	{ HTTPD_ANY,	"*",			error_404 },
	{ HTTPD_ANY,	NULL,			NULL },
};

int
main(void)
{
	unveil("html", "r");
	unveil("db", "r");
	pledge("stdio rpath", NULL);
	httpd_handle_request(handlers);
	return 0;
}
