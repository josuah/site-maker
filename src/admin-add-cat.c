#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "dat.h"
#include "fns.h"

int
main(void)
{
	size_t cat;
	char path[128];

	if(chdir("..") == -1)
		err(1, "chdir");
	if(unveil("data", "rwc"))
		err(1, "unveil");
	if(pledge("stdio rpath wpath cpath", nil))
		err(1, "pledge");

	if((cat = infonum(cgiget(), "cat", 1, 10000)) == 0)
		err(1, "$cat: %s", infoerr);

	snprintf(path, sizeof path, "data/cat%zu", cat);
	if(mkdir(path, 0775) == -1)
		cgierror(500, "mkdir %s: %s", path, strerror(errno));

	snprintf(path, sizeof path, "data/cat%zu/info.tmp", cat);
	//if(infostr())
		cgierror(500, "writing info to %s: %s", path, strerror(errno));

	cgiredir(307, "/cgi/admin-show-cat");

	return 0;
}
