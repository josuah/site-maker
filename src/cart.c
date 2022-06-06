#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "def.h"

#define EXPIRED "; expires=Thu, 01 Jan 1970 00:00:00 GMT"

typedef struct Fn Fn;

struct Fn
{
	char *name;
	void (*fn)(Info*, char*, char*, char*);
};

static void
add(Info *info, char *cart, char *ref, char *url)
{
	char *cm, *s, *new;

	if (!cart)cgi_fatal("no $cart");
	if (!ref)cgi_fatal("no $ref");

	/* check if already on the cart */
	for (s = cart; (cm = strchr(s, ',')); s = cm+1)
		if (strncmp(s, ref, cm-s) == 0)
			goto Exit;
	if (strcmp(s, ref) == 0)
		goto Exit;

	/* append the string at the end */
	s = (*cart == '\0') ? "" : ",";
	if (asprintf(&new, "%s%s%s", cart, s, ref) == -1)
		sysfatal("asprintf");
	info_set(cookies, "cart", new);
	cgi_redir(302, url);
	free(new);
Exit:
	cgi_redir(302, url);
}

static void
del(Info *info, char *cart, char *ref, char *url)
{
	size_t sz, n;
	int found;
	char *new, *item;

	sz = strlen(cart) + 1;
	n = found = 0;

	if ((new = calloc(sz, 1)) == NULL)
		sysfatal("malloc");

	while ((item = strsep(&cart, ","))) {
		if (strcmp(item, ref) == 0) {
			found = 1;
		}else{
			if (n++ > 0)
				strlcat(new, ",", sz);
			strlcat(new, item, sz);
		}
	}
	if (!found)
		cgi_fatal("cannot remove: %s not in cart", ref);
	info_set(cookies, "cart", *new ? new : EXPIRED);
	cgi_redir(302, url);
	free(new);
}

#define F(fn) { #fn, fn }
static Fn fmap[] = { F(add), F(del) };

static int
cmp(void const *v1, void const *v2)
{
	return strcasecmp(((Fn*)v1)->name, ((Fn*)v2)->name);
}

static void
cart(void)
{
	Info *info;
	Fn *f, q;
	char *url, *ref, *cart;

	info = cgi_get(cgi_cookies(NULL));

	if ((ref = info_str(info, "ref")) == NULL)
		cgi_fatal("no $ref");
	if ((q.name = info_str(info, "action")) == NULL)
		cgi_fatal("no $action");
	if ((cart = info_str(info, "cart")) == NULL)
		cart = "";
	if ((url = getenv("HTTP_REFERER")) == NULL)
		url = "/";
	if ((f = bsearch(&q, fmap, L(fmap), sizeof *fmap, cmp)) == NULL)
		cgi_fatal("action %s not found", q.name);
	f->fn(info, cart, ref, url);
}

int
main(int argc, char **argv)
{
	(void)argc;
	argv0 = argv[0];

	if (chdir("..") == -1)
		sysfatal("chdir");
	if (pledge("stdio", NULL) == -1)
		sysfatal("pledge");
	cart();
	return 0;
}
