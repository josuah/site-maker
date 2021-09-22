#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "dat.h"
#include "fns.h"

static int
cmp(void const *v1, void const *v2)
{
	InfoRow const *r1 = v1, *r2 = v2;

	return strcasecmp(r1->key, r2->key);
}

static int
add(Info *info, char *key, char *val)
{
	void *mem;

	info->len++;
	if((mem = realloc(info->vars, info->len * sizeof *info->vars)) == nil)
		return -1;
	info->vars = mem;
	info->vars[info->len-1].key = key;
	info->vars[info->len-1].val = val;
	return 0;
}

char *
infoget(Info *info, char *key)
{
	InfoRow *r, q = { .key = key };

	if(info == nil)
		return cgiquery(key);
	if((r = bsearch(&q, info->vars, info->len, sizeof *info->vars, cmp)))
		return r->val;
	return infoget(info->next, key);
}

/*
 * If key is already in info*, replace the value, otherwise append
 * it at the end.
 */
int
infoset(Info *info, char *key, char *val)
{
	InfoRow *r, query = { .key = key };
	int e;

	r = bsearch(&query, info->vars, info->len, sizeof *info->vars, cmp);
	if(r != nil){
		r->val = val;
		return 0;
	}
	e = add(info, key, val);
	qsort(info->vars, info->len, sizeof *info->vars, cmp);
	return e;
}

Info *
infopop(Info *info)
{
	Info *next;

	next = info->next;
	free(info->buf);
	free(info);
	return next;
}

void
infofree(Info *info)
{
	while((info = infopop(info)) != nil);
}

Info *
infonew(Info *next, char *path)
{
	Info *info;
	char *buf = nil, *line = nil, *tail, *key;
	void *mem;

	if((info = calloc(sizeof *info, 1)) == nil)
		goto Err;
	memset(info, 0, sizeof *info);

	if((tail = buf = fopenread(path)) == nil)
		goto Err;
	while((line = strsep(&tail, "\n")) != nil){
		if(line[0] == '\0')
			break;
		key = strsep(&line, ":");
		if(line == nil || *line++ != ' ')
			goto Err;
		if(add(info, key, line) == -1)
			goto Err;
	}
	if(add(info, "Text", tail ? tail : "") == -1)
		goto Err;
	qsort(info->vars, info->len, sizeof *info->vars, cmp);
	info->next = next;
	return info;
Err:
	free(buf);
	infofree(info);
	return nil;
}

Info *
infodefault(void)
{
	Info *info = nil, *new;
	size_t cat, item;
	char path[128];
	char const *errstr;

	if((new = infonew(info, "data/info")) == nil)
		goto Err;
	info = new;

	if((cat = cgiquerynum("cat", 1, 100000, &errstr)) == 0)
		goto End;

	snprintf(path, sizeof path, "data/cat%zu/info", cat);
	if((new = infonew(info, path)) == nil)
		goto Err;
	info = new;

	if((item = cgiquerynum("item", 1, 100000, &errstr)) == 0)
		goto End;

	snprintf(path, sizeof path, "data/cat%zu/item%zu/info", cat, item);
	if((new = infonew(info, path)) == nil)
		goto Err;
	info = new;

End:
	return info;
Err:
	infofree(info);
	return NULL;
}
