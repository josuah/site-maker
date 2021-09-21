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

void
sort(Info *info)
{
	if(!info->sorted)
		qsort(info->vars, info->len, sizeof *info->vars, cmp);
	info->sorted = 1;
}

static int
add(Info *info, char *key, char *val)
{
	void *mem;

	info->sorted = 0;
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
	InfoRow *r, query = { .key = key };

	if(info == NULL)
		return NULL;
	r = bsearch(&query, info->vars, info->len, sizeof *info->vars, cmp);
	warn("%s: query=%s result=%p", __func__, key, r);
	return r == NULL ? NULL : r->val;
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
	sort(info);
	return e;
}

Info *
infoopen(char *path)
{
	Info *info = NULL;
	char *buf = NULL, *line = NULL, *tail, *key;
	void *mem;

	if((tail = buf = fopenread(path)) == NULL)
		goto Err;
	if((info = calloc(1, sizeof *info)) == NULL)
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
	sort(info);
	return info;
Err:
	free(buf);
	infoclose(info);
	return NULL;
}

void
infoclose(Info *info)
{
	free(info->buf);
	free(info);
}
