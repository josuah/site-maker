#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dat.h"
#include "fns.h"

static char *fields[] = { "Cat-Name", NULL };

size_t
newcat(void)
{
	char path[128];
	size_t i;

	for(i = 1; i < SIZE_MAX; i++){
		snprintf(path, sizeof path, "data/cat%zu", i);
		if(access(path, F_OK) == -1 && errno == ENOENT)
			return i;
	}
	assert(!"not reached");
	return 0;
}

int
main(void)
{
	Info *info, *form;
	size_t cat;
	char path[128], dest[128], *miss;
	pid_t pid;

	if(chdir("..") == -1)
		err(1, "chdir");
	if(unveil("data", "rwc")
	|| unveil("tmp", "rwc"))
		err(1, "unveil");
	if(pledge("stdio rpath wpath cpath", nil))
		err(1, "pledge");

	pid = getpid();
	cat = newcat();
	form = cgipost(nil);
	info = cgiget(nil);

	if((miss = infomiss(form, fields)))
		cgierror(400, "no $%s", miss);

	snprintf(path, sizeof path, "tmp/%d", pid);
	mkdir(path, 0760);

	snprintf(path, sizeof path, "tmp/%d/cat%zu", pid, cat);
	mkdir(path, 0760);

	snprintf(path, sizeof path, "tmp/%d/cat%zu/info", pid, cat);
	if(infowrite(form, path))
		cgierror(500, "writing info to %s: %s", path, strerror(errno));

	snprintf(path, sizeof path, "tmp/%d/cat%zu", pid, cat);
	snprintf(dest, sizeof dest, "data/cat%zu", cat);
	if(rename(path, dest) == -1)
		cgierror(500, "%s -> %s: %s", path, dest, strerror(errno));

	snprintf(path, sizeof path, "tmp/%d", pid);
	rmdir(path);

	infofree(form);
	infofree(info);
	cgiredir(307, "/cgi/show?page=admincat");
	return 0;
}
