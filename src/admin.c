#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
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
	Info *(*fn)(Info *, char *);
};

static void
xrename(char *src, char *dst)
{
	if(rename(src, dst) == -1)
		cgifatal("%s -> %s: %s", src, dst, strerror(errno));
}

static size_t
newid(char *ref, char *suffix)
{
	char path[256];
	size_t i;

	for(i = 1; i < SIZE_MAX; i++){
		snprintf(path, sizeof path, "%s%zi%s", ref, i, suffix);
		if(access(path, F_OK) == -1){
			if(errno == ENOENT){
				return i;
			}else{
				cgifatal("scanning %s: %s",
				  path, strerror(errno));
			}
		}
	}
	assert(!"not reached");
	return 0;
}

static Info *
addinfo(Info *info, char *ref)
{
	char *leaf; char path[256], dest[256];
	pid_t pid;
	size_t id;

	info = cgipost(info);
	pid = getpid();
	id = newid(ref, "");
	if((leaf = strrchr(ref, '/')))
		leaf++;

	snprintf(path, sizeof path, "tmp/%i", pid);
	mkdir(path, 0770);

	snprintf(path, sizeof path, "tmp/%i/%s%zu", pid, leaf, id);
	mkdir(path, 0770);

	snprintf(path, sizeof path, "tmp/%i/%s%zu/info", pid, leaf, id);
	if(infowrite(info, path))
		cgifatal("writing info to %s: %s", path, strerror(errno));

	snprintf(path, sizeof path, "tmp/%i/%s%zu", pid, leaf, id);
	snprintf(dest, sizeof dest, "%s%zu", ref, id);
	xrename(path, dest);

	snprintf(path, sizeof path, "tmp/%i", pid);
	rmdir(path);

	return info;
}

static Info *
editinfo(Info *info, char *ref)
{
	char path[256];

	info = cgipost(info);

	snprintf(path, sizeof path, "%s/info", ref);
	if(infowrite(info, path))
		cgifatal("writing info to %s: %s", path, strerror(errno));

	return info;
}

static Info *
delinfo(Info *info, char *ref)
{
	DIR *dp;
	char path[1024];

	if((dp = opendir(ref)) == nil)
		cgifatal("%s: cannot open directory", ref);
	readdir(dp), readdir(dp), readdir(dp); /* ".", "..", "info" */
	if(readdir(dp) != nil)
		cgifatal("%s: element not empty", ref);
	closedir(dp);

	snprintf(path, sizeof path, "%s/info", ref);
	if(unlink(path) == -1)
		cgifatal("deleting %s: %s", path, strerror(errno));

	if((rmdir(ref)) == -1)
		cgifatal("deleting %s: %s", path, strerror(errno));

	return info;
}

static void
swap(char *ref, int off)
{
	char *p, **pp, path[256];
	pid_t pid;
	glob_t gl;

	pid = getpid();

	if((p = strrchr(ref, '/')) == nil)
		cgifatal("invalid $ref");
	p += strcspn(p, "0123456789");
	if(!isdigit(*p))
		cgifatal("invalid $ref");

	snprintf(path, sizeof path, "%.*s*", (int)(p - ref), ref);
        if(glob(path, 0, nil, &gl) != 0)
		goto End;

        for(pp = gl.gl_pathv; *pp; pp++)
		if(strcmp(*pp, ref) == 0)
			break;
	if(*pp == nil)
		cgifatal("%s not found", ref);
	if(pp + off < gl.gl_pathv || pp[off] == nil)
		goto End;

	snprintf(path, sizeof path, "tmp/%i", pid);
	mkdir(path, 0770);

	snprintf(path, sizeof path, "tmp/%i/pivot", pid);
	xrename(*pp, path);
	xrename(pp[off], *pp);
	xrename(path, pp[off]);
End:
	rmdir(path);
	globfree(&gl);
}

static Info *
swapup(Info *info, char *ref)
{
	swap(ref, -1);
	return info;
}

static Info *
swapdown(Info *info, char *ref)
{
	swap(ref, +1);
	return info;
}

static Info *
addimg(Info *info, char *ref)
{
	char path[256], dest[256];
	size_t id;

	cgifile(path, sizeof path);
	id = newid(ref, ".jpg");

	snprintf(dest, sizeof dest, "%s%zu%s", ref, id, ".jpg");
	xrename(path, dest);

	return info;
}

static Info *
delimg(Info *info, char *ref)
{
	char path[1024];

	snprintf(path, sizeof path, "%s", ref);
	if(unlink(path) == -1)
		cgifatal("deleting %s", ref);

	return info;
}

static int
cmp(void const *v1, void const *v2)
{
	return strcasecmp(((Fn *)v1)->name, ((Fn *)v2)->name);
}

#define F(fn) { #fn, fn }
Fn fnmap[] = {
	/* sorted for bearch */
        F(addimg), F(addinfo), F(delimg), F(delinfo), F(editinfo),
        F(swapdown), F(swapup)
};

static void
admin(void)
{
	Info *info;
	char *ref, *url;
	Fn fq, *fn;

	info = cgiget(nil);

	if((ref = infostr(info, "ref")) == nil)
		cgifatal("no $ref");
	if((fq.name = infostr(info, "action")) == nil)
		cgifatal("no $action");

	if ((fn = bsearch(&fq, fnmap, L(fnmap), sizeof *fnmap, cmp)) == nil)
		cgifatal("action %s not found", fq.name);
	info = fn->fn(info, ref);

	if((url = getenv("HTTP_REFERER")) == nil)
		url = "/";
	infofree(info);
	cgiredir(302, "%s", url);
}

int
main(void)
{
	if(chdir("..") == -1)
		sysfatal("chdir");
	if(unveil("data", "rwc")
	|| unveil("tmp", "rwc"))
		sysfatal("unveil");
	if(pledge("stdio rpath wpath cpath", nil))
		sysfatal("pledge");
	admin();
	return 0;
}
