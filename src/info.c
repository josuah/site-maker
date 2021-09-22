#include <strings.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
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
	if((mem = realloc(info->vars, info->len * sizeof *info->vars)) == NULL)
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

	if(info == NULL)
		return NULL;
	if((r = bsearch(&q, info->vars, info->len, sizeof *info->vars, cmp)))
		return r->val;
	return cgiquery(key);
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
	if(r != NULL){
		r->val = val;
		return 0;
	}
	e = add(info, key, val);
	qsort(info->vars, info->len, sizeof *info->vars, cmp);
	return e;
}

Info *
infonew(char *path)
{
	Info *info;
	char *buf = NULL, *line = NULL, *tail, *key;
	void *mem;

	if((info = calloc(sizeof *info, 1)) == NULL)
		goto Err;
	memset(info, 0, sizeof *info);

	if((tail = buf = fopenread(path)) == NULL)
		goto Err;
	while((line = strsep(&tail, "\n")) != NULL && tail != NULL){
		if(line[0] == '\0')
			break;
		key = strsep(&line, ":");
		if(line == NULL || *line++ != ' ')
			goto Err;
		if(add(info, key, line) == -1)
			goto Err;
	}
	if(add(info, "Text", line) == -1)
		goto Err;
	qsort(info->vars, info->len, sizeof *info->vars, cmp);
	return info;
Err:
	free(buf);
	infofree(info);
	return NULL;
}

void
infofree(Info *info)
{
	free(info->buf);
	free(info);
}
