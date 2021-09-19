#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include "fns.h"

int
main(void)
{
	struct dirent **dlist;

	if(unveil("html", "r") == -1 || unveil("data", "r") == -1)
		errx(1, "unveil");
	if(pledge("stdio rpath", NULL) == -1)
		errx(1, "pledge");

	htmlcat("html/head.html");
	htmlcategories();
	htmlcat("html/foot.html");

	return 0;
}
