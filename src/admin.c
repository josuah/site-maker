#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "def.h"

typedef struct Fn Fn;

struct Fn {
	char *name;
	void (*fn)(Info *, char *);
};

static size_t
newid(char *ref, char *suffix)
{
	char path[128];
	size_t i;

	for(i = 1; i < SIZE_MAX; i++){
		snprintf(path, sizeof path, "%s%zi%s", ref, i, suffix);
		if(access(path, F_OK) == -1){
			if(errno == ENOENT){
				return i;
			}else{
				cgierror(500, "scanning %s: %s",
				  path, strerror(errno));
			}
		}
	}
	assert(!"not reached");
	return 0;
}

static void
addinfo(Info *info, char *ref)
{
	char *leaf; char path[128], dest[128];
	pid_t pid;
	size_t id;

	pid = getpid();
	id = newid(ref, "");
	if((leaf = strrchr(ref, '/')))
		leaf++;

	snprintf(path, sizeof path, "tmp/%d", pid);
	mkdir(path, 0760);

	snprintf(path, sizeof path, "tmp/%d/%s%zu", pid, leaf, id);
	mkdir(path, 0760);

	snprintf(path, sizeof path, "tmp/%d/%s%zu/info", pid, leaf, id);
	warn("info=%s", path);
	if(infowrite(info, path))
		cgierror(500, "writing info to %s: %s", path, strerror(errno));

	snprintf(path, sizeof path, "tmp/%d/%s%zu", pid, leaf, id);
	snprintf(dest, sizeof dest, "%s%zu", ref, id);
	if(rename(path, dest) == -1)
		cgierror(500, "%s -> %s: %s", path, dest, strerror(errno));

	snprintf(path, sizeof path, "tmp/%d", pid);
	rmdir(path);
}

static void
editinfo(Info *info, char *ref)
{
	char path[128];

	snprintf(path, sizeof path, "%s/info", ref);
	if(infowrite(info, path))
		cgierror(500, "writing info to %s: %s", path, strerror(errno));
}

static void
delinfo(Info *info, char *ref)
{
	char path[1024];
	(void)info;

	snprintf(path, sizeof path, "%s/info", ref);
	if(unlink(path) == -1)
		cgierror(500, "deleting %s: %s", path, strerror(errno));

	snprintf(path, sizeof path, "%s", ref);
	if((rmdir(path)) == -1)
		cgierror(500, "deleting %s: %s", path, strerror(errno));
}

static void
addimg(Info *info, char *ref)
{
	(void)info;
	(void)ref;
}

static void
delimg(Info *info, char *ref)
{
	char path[1024];
	(void)info;

	snprintf(path, sizeof path, "%s", ref);
	if(unlink(path) == -1)
		cgierror(500, "deleting %s", ref);
}

static int
cmp(void const *v1, void const *v2)
{
	return strcasecmp(((Fn *)v1)->name, ((Fn *)v2)->name);
}

#define F(fn) { #fn, fn }
Fn fnmap[] = {
	/* sorted for bearch */
	F(addimg), F(addinfo), F(delimg), F(delinfo), F(editinfo)
};

int
main(void)
{
	Info *info;
	char *ref, *url;
	Fn fq, *fn;

	if(chdir("..") == -1)
		err(1, "chdir");
	if(unveil("data", "rwc")
	|| unveil("tmp", "rwc"))
		err(1, "unveil");
	if(pledge("stdio rpath wpath cpath", nil))
		err(1, "pledge");

	info = cgipost(cgiget(nil));

	if((ref = infostr(info, "ref")) == nil)
		cgierror(400, "no $ref");
	if((fq.name = infostr(info, "action")) == nil)
		cgierror(400, "no $action");

	for(size_t i = 0; i < L(fnmap); i++)
		warn("action=%s name=%s", fq.name, fnmap[i].name);

	if ((fn = bsearch(&fq, fnmap, L(fnmap), sizeof *fnmap, cmp)) == nil)
		cgierror(400, "action %s not found", fq.name);
	fn->fn(info, ref);

	if((url = getenv("HTTP_REFERER")) == nil)
		url = "/";
	infofree(info);
	cgiredir(307, "%s", url);
	return 0;
}
