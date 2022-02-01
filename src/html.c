#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "def.h"

typedef struct Fn Fn;

struct Fn
{
	char *name;
	void (*fn)(Info*); 
};

void
htmlprint(char *s)
{
	for(; *s != '\0'; s++){
		switch(*s){
		case '<':
			fputs("&lt;", stdout);
			break;
		case '>':
			fputs("&gt;", stdout);
			break;
		case '"':
			fputs("&quot;", stdout);
			break;
		case '\'':
			fputs("&#39;", stdout);
			break;
		case '&':
			fputs("&amp;", stdout);
			break;
		default:
			fputc(*s, stdout);
		}
	}
}

static void
list(Info *info, char *dir, char *expr, char *html)
{
	Info tmp;
	Irow row;
	glob_t gl;
	char path[256], **pp, *sl;
	int pop;

	if (*expr == '/')
		snprintf(path, sizeof path, "data%s", expr);
	else
		snprintf(path, sizeof path, "data/%s/%s", dir, expr);

	if(glob(path, 0, NULL, &gl) != 0)
		goto End;

	for(pp = gl.gl_pathv; *pp; pp++){
		sl = strrchr(*pp, '/');
		if(sl != NULL && strcmp(sl, "/info") == 0){
			if((info = inforead(info, *pp)) == NULL)
				sysfatal("parsing %s", *pp);
			*sl = '\0';
			pop = 1;
		}else{
			pop = 0;
		}

		row.key = "ref";
		row.val = strchr(*pp, '/');
		row.val = (row.val == NULL) ? "" : row.val + 1;

		tmp.vars = &row;
		tmp.next = info;
		tmp.len = 1;
		tmp.buf = NULL;

		snprintf(path, sizeof path, "html/elem.%s.html", html);
		htmltemplate(path, &tmp);

		if(pop)
			info = infopop(info);
	}
End:
	globfree(&gl);
}

static void
cart(Info *info)
{
	Info *new;
	char *s, *ref, path[256];

	if((s = infostr(info, "cart")) == NULL){
		htmltemplate("html/elem.cartempty.html", info);
		return;
	}
	while((ref = strsep(&s, ","))){
		snprintf(path, sizeof path, "%s/info", ref);
		if((new = inforead(info, path)) == NULL){
			htmltemplate("html/elem.cartmissing.html", info);
			continue;
		}
		infoset(new, "ref", ref);
		htmltemplate("html/elem.cart.html", new);
		infopop(new);
	}
}

static void
cartcount(Info *info)
{
	char *cart;
	usize count;

	count = 0;
	if((cart = infostr(info, "cart"))){
		count += (*cart != '\0');
		while((cart = strchr(cart, ',')))
			cart++, count++;
	}
	fprintf(stdout, "%zu", count);
}

static void
now(Info *info)
{
	fprintf(stdout, "%lld", (vlong)time(NULL));
}

static void
parent(Info *info)
{
	char buf[256], *ref, *sl;

	if((ref = infostr(info, "ref")) == NULL)
		sysfatal("no $ref");
	strlcpy(buf, ref, sizeof buf);
	if((sl = strrchr(buf, '/')))
		*sl = '\0';
	fputs(buf, stdout);
}

static void
strip1(Info *info)
{
	char *ref, *new;

	if((ref = infostr(info, "ref")) == NULL)
		sysfatal("no $ref");
	new = strchr(ref, '/');
	fputs((new == NULL) ? "" : new + 1, stdout);
}

#define F(name) { #name, name }
static Fn fmap[] = { F(cart), F(cartcount), F(now), F(parent), F(strip1) };

static int
cmp(const void *v1, const void *v2)
{
	return strcasecmp(((Fn *)v1)->name, ((Fn *)v2)->name);
}

static char*
next(char *head, char **tail)
{
	char *beg, *end;

	if((beg = strstr(head, "{{")) == NULL
	|| (end = strstr(beg, "}}")) == NULL)
		return NULL;
	*beg = *end = '\0';
	*tail = end + strlen("}}");
	return beg + strlen("{{");
}

/*
 * takes a list of Info* arguments by order of precedence
 */
void
htmltemplate(char *htmlpath, Info *info)
{
	FILE *fp;
	Fn *f, q;
	usize sz;
	char *line, *head, *tail, *sp, *ref, *val;

	sz = 0;
	line = NULL;

	if((fp = fopen(htmlpath, "r")) == NULL)
		sysfatal("opening template %s", htmlpath);

	while(getline(&line, &sz, fp) > 0){
		head = tail = line;
		for(; (q.name = next(head, &tail)); head = tail){
			fputs(head, stdout);

			if((f = bsearch(&q, fmap, L(fmap), sizeof *fmap, cmp))){
				f->fn(info);

			}else if((sp = strchr(q.name, ' '))){
				*sp = '\0';
				if((ref = infostr(info, "ref")) == NULL)
					sysfatal("no $ref");
				list(info, ref, sp + 1, q.name);

			}else if((val = infostr(info, q.name))){
				htmlprint(val);

			}else{
				fprintf(stdout, "{{error:%s}}", q.name);
			}
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}
