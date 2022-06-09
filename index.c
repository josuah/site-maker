#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "libhttpd.h"

struct httpd_var_list website;

static void
loop(struct httpd_var_list *vars, char *key, void (*fn)(struct httpd_var_list *vars))
{
	char *val, list[1024];

	if ((val = httpd_get_var(vars, key)) == NULL)
		httpd_fatal("looping over $%s: no such variable", key);
	strlcpy(list, val, sizeof list);
	for (char *s = list, *ref = list; (s = strsep(&ref, " ")) != NULL;) {
		struct httpd_var_list elem = {0};
		char path[64];

		if (*s == '\0')
			continue;
		snprintf(path, sizeof path, "db/%s/%s", key, s);
		httpd_read_var_list(&elem, path);
		fn(&elem);
	}
}

static ino_t
get_inode(char *path)
{
	struct stat st = {0};

	if (stat(path, &st) == -1)
		httpd_fatal("stat %s: %s", path, strerror(errno));
	return st.st_ino;
}

static void
add_id(char *parent, char *child, uint32_t id)
{
	struct httpd_var_list vars = {0};
	char new[1024], path[64], *val;

	snprintf(path, sizeof path, "db/%s", parent);
	httpd_read_var_list(&vars, path);
	if ((val = httpd_get_var(&vars, child)) == 0)
		httpd_fatal("adding id %d to %s: no $%s", parent, id, child);

	snprintf(new, sizeof new, (*val == '\0') ? "%s%d" : "%s %d", val, id);
	httpd_set_var(&vars, child, new);
	httpd_write_var_list(&vars, path);
	httpd_free_var_list(&vars);
}

static void
del_string(char *new, size_t sz, char *old, char *id)
{
	char *cp;
	size_t len = strlen(id);

	for (cp = old; (cp = strstr(cp, id));)
		if (cp[len] == ' ' || cp[len] == '\0')
			goto found;
	httpd_fatal("could not find '%s' to delete in '%s' in $%s", id, old);
found:
	len += (cp[len] == ' ');
	snprintf(new, sz, "%.*s%s", (int)(cp - old), old, cp + len);
	len = strlen(new);
	if (new[len] == ' ')
		new[len] = '\0';
}

static void
del_id(char *parent, char *child, char *id)
{
	struct httpd_var_list vars = {0};
	char path[64], *val, new[1024];

	snprintf(path, sizeof path, "db/%s", parent);
	httpd_read_var_list(&vars, path);
	if ((val = httpd_get_var(&vars, child)) == 0)
		httpd_fatal("removing id %d from %s: no $%s", parent, id, child);

	del_string(new, sizeof new, val, id);
	httpd_set_var(&vars, child, new);
	httpd_write_var_list(&vars, path);
}

static void
payload_as_child(char *parent, char *child)
{
	struct httpd_var_list vars = {0};
	char tmp[64], path[64];
	uint32_t id;

	snprintf(tmp, sizeof tmp, "tmp/%d", getpid());
	httpd_parse_payload(&vars);
	httpd_write_var_list(&vars, tmp);
	id = get_inode(tmp);

	snprintf(path, sizeof path, "db/%s/%d", child, id);
	if (rename(tmp, path) == -1)
		httpd_fatal("rename %s to %s: %s", tmp, path, strerror(errno));

	add_id(parent, child, id);
}

static void
website_loop_nav_category(struct httpd_var_list *vars)
{
	httpd_template("html/website-nav-category.html", vars);
}

static void
website_head(char *page_name)
{
	httpd_read_var_list(&website, "db/website");
	httpd_set_var(&website, "page-name", page_name);
	httpd_send_headers(200, "text/html");

	httpd_template("html/website-head.html", &website);

	printf("<nav>\n");
	loop(&website, "category", website_loop_nav_category);
	printf("<a href=\"/cart/\">Panier</a>\n");
	printf("</nav>\n");

	printf("<main>\n");
}

static void
website_foot(void)
{
	printf("</main>\n");
}

static void
error_404(char **matches)
{
	(void)matches;

	website_head("404");
	httpd_template("html/404.html", &website);
	printf("<code>");
	httpd_print_html(matches[0]);
	printf("</code>\n");
	website_foot();
}

static void
home_loop_category(struct httpd_var_list *category)
{
	httpd_template("html/home-category.html", category);
}

static void
page_home(char **matches)
{
	(void)matches;

	website_head("Accueil");
	httpd_template("html/home.html", &website);
	loop(&website, "category", home_loop_category);
	website_foot();
}

static void
category_loop_item(struct httpd_var_list *item)
{
	httpd_template("html/category-item.html", item);
}

static void
page_category(char **matches)
{
	struct httpd_var_list category = {0};
	char path[64];

	snprintf(path, sizeof path, "db/category/%s", matches[0]);
	httpd_read_var_list(&category, path);
	website_head("Accueil");
	httpd_template("html/category.html", &category);
	loop(&category, "item", category_loop_item);
	website_foot();
}

static void
page_item(char **matches)
{
	struct httpd_var_list item = {0};
	char path[64];

	snprintf(path, sizeof path, "db/item/%s", matches[0]);
	httpd_read_var_list(&item, path);
	website_head("Accueil");
	httpd_template("html/item.html", &item);
	website_foot();
}

static void
cart_loop_item(struct httpd_var_list *item)
{
	httpd_template("html/cart-item.html", item);
}

static void
page_cart(char **matches)
{
	struct httpd_var_list cookies = {0};
	(void)matches;

	website_head("Accueil");

	httpd_parse_cookies(&cookies);

	if (httpd_get_var(&cookies, "item") == NULL) {
		httpd_template("html/cart-empty.html", &website);
	} else {
		loop(&cookies, "item", cart_loop_item);
		httpd_template("html/cart-checkout.html", &website);
	}

	website_foot();
}

static void
page_cart_add(char **matches)
{
	struct httpd_var_list cookies = {0};
	char *val, new[1024];

	httpd_parse_cookies(&cookies);
	if ((val = httpd_get_var(&cookies, "item")) == NULL) {
		httpd_set_var(&httpd_cookies, "item", matches[0]);
	} else {
		snprintf(new, sizeof new, "%s %s", val, matches[0]);
		httpd_set_var(&httpd_cookies, "item", new);
	}
	httpd_redirect(303, "/cart/");
}

static void
page_cart_del(char **matches)
{
	struct httpd_var_list cookies = {0};
	char new[1024], *val;

	httpd_parse_cookies(&cookies);
	if ((val = httpd_get_var(&cookies, "item")) == NULL)
		httpd_fatal("no $item cookie");
	del_string(new, sizeof new, val, matches[0]);
	httpd_set_var(&httpd_cookies, "item", new);
	httpd_redirect(303, "/cart/");
}

static char *category_file;

static void
admin_loop_item(struct httpd_var_list *item)
{
	httpd_set_var(item, "category.file", category_file);
	httpd_template("html/admin-item-edit.html", item);
}

static void
admin_loop_category(struct httpd_var_list *category)
{
	httpd_template("html/admin-category-edit.html", category);
	category_file = httpd_get_var(category, "file");
	loop(category, "item", admin_loop_item);
	httpd_template("html/admin-item-add.html", category);
}

static void
page_admin(char **matches)
{
	struct httpd_var_list category = {0};
	(void)matches;

	website_head("Administration");
	loop(&website, "category", admin_loop_category);
	httpd_template("html/admin-category-add.html", &category);
	website_foot();
}

static void
page_admin_item_add(char **matches)
{
	char parent[64];

	snprintf(parent, sizeof parent, "category/%s", matches[0]);
	payload_as_child(parent, "item");
	httpd_redirect(303, "/admin/");
}

static void
page_admin_item_edit(char **matches)
{
	struct httpd_var_list vars = {0};
	char path[64];

	snprintf(path, sizeof path, "db/item/%s", matches[0]);
	httpd_parse_payload(&vars);
	httpd_write_var_list(&vars, path);
	httpd_redirect(303, "/admin/");
}

static void
page_admin_item_del(char **matches)
{
	char parent[64];

	snprintf(parent, sizeof parent, "category/%s", matches[0]);
	del_id(parent, "item", matches[1]);
	httpd_redirect(303, "/admin/");
}

static void
page_admin_category_add(char **matches)
{
	(void)matches;

	payload_as_child("website", "category");
	httpd_redirect(303, "/admin/");
}

static void
page_admin_category_edit(char **matches)
{
	struct httpd_var_list vars = {0};
	char path[64];

	snprintf(path, sizeof path, "db/category/%s", matches[0]);
	httpd_parse_payload(&vars);
	httpd_write_var_list(&vars, path);
	httpd_redirect(303, "/admin/");
}

static void
page_admin_category_del(char **matches)
{
	del_id("website", "category", matches[0]);
	httpd_redirect(303, "/admin/");
}

static struct httpd_handler handlers[] = {
	{ HTTPD_GET,	"/",				page_home },
	{ HTTPD_GET,	"/category/*/",			page_category },
	{ HTTPD_GET,	"/item/*/",			page_item },
	{ HTTPD_GET,	"/cart/",			page_cart },
	{ HTTPD_POST,	"/cart/add/*/",			page_cart_add },
	{ HTTPD_POST,	"/cart/del/*/",			page_cart_del },
	{ HTTPD_GET,	"/admin/",			page_admin },
	{ HTTPD_POST,	"/admin/category/add/",		page_admin_category_add },
	{ HTTPD_POST,	"/admin/category/del/*/",	page_admin_category_del },
	{ HTTPD_POST,	"/admin/item/add/*/",		page_admin_item_add },
	{ HTTPD_POST,	"/admin/item/edit/*/",		page_admin_item_edit },
	{ HTTPD_POST,	"/admin/item/del/*/*/",		page_admin_item_del },
	{ HTTPD_ANY,	"*",				error_404 },
	{ HTTPD_ANY,	NULL,				NULL },
};

int
main(void)
{
	/* restrict allowed paths */
	unveil("html", "r");
	unveil("tmp", "rwc");
	unveil("db", "rwc");

	/* restrict allowed system calls */
	pledge("stdio rpath wpath cpath", NULL);

	/* handle the request with the handlers */
	httpd_handle_request(handlers);
	return 0;
}
