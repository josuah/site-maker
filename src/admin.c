#include <assert.h>
#include <ctype.h>
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
	warn("%s -> %s", src, dst);
	if(rename(src, dst) == -1)
		cgierror(500, "%s -> %s: %s", src, dst, strerror(errno));
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
				cgierror(500, "scanning %s: %s",
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
		cgierror(500, "writing info to %s: %s", path, strerror(errno));

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
		cgierror(500, "writing info to %s: %s", path, strerror(errno));

	return info;
}

static Info *
delinfo(Info *info, char *ref)
{
	struct stat st;
	char path[1024];

	if(stat(ref, &st) == -1)
		cgierror(500, "stat %s: %s", ref, strerror(errno));
	if(st.st_nlink > 2 /* ".", "info" */)
		cgierror(500, "%s: element not empty", ref);

	snprintf(path, sizeof path, "%s/info", ref);
	if(unlink(path) == -1)
		cgierror(500, "deleting %s: %s", path, strerror(errno));

	if((rmdir(ref)) == -1)
		cgierror(500, "deleting %s: %s", path, strerror(errno));

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
		cgierror(400, "invalid $ref");
	p += strcspn(p, "0123456789");
	if(!isdigit(*p))
		cgierror(400, "invalid $ref");

	snprintf(path, sizeof path, "%.*s*", (int)(p - ref), ref);
        if(glob(path, 0, nil, &gl) != 0)
		goto End;

        for(pp = gl.gl_pathv; *pp; pp++)
		if(strcmp(*pp, ref) == 0)
			break;
	if(*pp == nil)
		cgierror(500, "%s not found", ref);
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
		cgierror(500, "deleting %s", ref);

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

int
main(void)
{
	Info *info = nil;
	char *ref, *url;
	Fn fq, *fn;

	if(chdir("..") == -1)
		err(1, "chdir");
	if(unveil("data", "rwc")
	|| unveil("tmp", "rwc"))
		err(1, "unveil");
	if(pledge("stdio rpath wpath cpath", nil))
		err(1, "pledge");

	info = cgiget(nil);

	if((ref = infostr(info, "ref")) == nil)
		cgierror(400, "no $ref");
	if((fq.name = infostr(info, "action")) == nil)
		cgierror(400, "no $action");

	if ((fn = bsearch(&fq, fnmap, L(fnmap), sizeof *fnmap, cmp)) == nil)
		cgierror(400, "action %s not found", fq.name);
	info = fn->fn(info, ref);

	if((url = getenv("HTTP_REFERER")) == nil)
		url = "/";
	infofree(info);
	cgiredir(302, "%s", url);
	return 0;
}
