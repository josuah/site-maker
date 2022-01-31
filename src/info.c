#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "def.h"

/*
 * Simple key-value storage with a fallback mechanism:
 * If the current (Info *) does not have the key queried, the lookup
 * will continue on the (Info *)->next field.
 * 
 * A RFC822-like format permits to store and read the data on a file:
 *	First-Key-Name: One needle
 *	Second-Key-Name: Two haystacks
 *	Third-Key-Name: Three magnets
 * 
 *	Extra multiline text that will be read into the "Text" key.
 * 
 * This permits to mix key-values from various sources such as
 * environment variables, headers, cookies, get/post params, data
 * files...
 */

char const *infoerr = "(no message)";
#define Err(msg) { infoerr = msg; goto Err; }

static int
cmp(const void *v1, const void *v2)
{
	return strcasecmp(((Irow *)v1)->key, ((Irow *)v2)->key);
}

void
infosort(Info *info)
{
	qsort(info->vars, info->len, sizeof *info->vars, cmp);
}

void
infoadd(Info *info, char *key, char *val)
{
	void *mem;

	info->len++;
	if((mem = realloc(info->vars, info->len * sizeof *info->vars)) == NULL)
		 sysfatal("realloc");
	info->vars = mem;
	info->vars[info->len-1].key = key;
	info->vars[info->len-1].val = val;
}

char*
infostr(Info *info, char *key)
{
	Irow *r, q;

	q.key = key;

	if(info == NULL)
		return NULL;
	if((r = bsearch(&q, info->vars, info->len, sizeof *info->vars, cmp)))
		return r->val;
	return infostr(info->next, key);
}

vlong
infonum(Info *info, char *key, vlong min, vlong max)
{
	char *val;

	if((val = infostr(info, key)) == NULL)
		Err("key not found");
	return strtonum(val, min, max, &infoerr);
Err:
	return 0;
}

/*
 * If key is already in info*, replace the value, otherwise append
 * it at the end.
 */
void
infoset(Info *info, char *key, char *val)
{
	Irow *r, q;

	q.key = key;
	r = bsearch(&q, info->vars, info->len, sizeof *info->vars, cmp);
	if(r != NULL){
		r->val = val;
		return;
	}
	infoadd(info, key, val);
	infosort(info);
}

Info*
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
	while((info = infopop(info)) != NULL);
}

Info*
inforead(Info *next, char *path)
{
	Info *info;
	char *buf, *line, *tail, *key;

	buf = line = NULL;

	if((info = calloc(sizeof *info, 1)) == NULL)
		Err(strerror(errno));
	memset(info, 0, sizeof *info);

	tail = buf = fopenread(path);
	if(buf == NULL)
		Err(strerror(errno));
	while((line = strsep(&tail, "\n")) != NULL){
		if(line[0] == '\0')
			break;
		key = strsep(&line, ":");
		if(line == NULL || *line++ != ' ')
			Err("missing ': ' separator");
		infoadd(info, key, line);
	}
	infoadd(info, "Text", tail ? tail : "");
	infosort(info);

	info->next = next;
	return info;
Err:
	free(buf);
	infofree(info);
	return NULL;
}

int
infowrite(Info *info, char *dst)
{
	FILE *fp;
	Irow *row;
	usize n;
	char path[1024], *text;

	text = NULL;

	snprintf(path, sizeof path, "%s.tmp", dst);
	if((fp = fopen(path, "w")) == NULL)
		Err("opening info file for writing")

	for(row = info->vars, n = info->len; n > 0; row++, n--){
		if(strcasecmp(row->key, "Text") == 0){
			text = text ? text : row->val;
			continue;
		}
		assert(strchr(row->key, '\n') == NULL);
		assert(strchr(row->val, '\n') == NULL);
		fprintf(fp, "%s: %s\n", row->key, row->val);
	}
	fprintf(fp, "\n%s", text ? text : "");

	fclose(fp);
	if(rename(path, dst) == -1)
		Err("applying changes to info file");
	return 0;
Err:
	if(fp) fclose(fp);
	return -1;
}
