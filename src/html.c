#include <errno.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "def.h"

void
htmlprint(char *s)
{
	for(; *s != '\0'; s++){
		switch(*s){
		case '<':
			fputs("&lt;", stdout);
			break;
		case '>':
			fputs("&gt;", stdout);
			break;
		case '"':
			fputs("&quot;", stdout);
			break;
		case '\'':
			fputs("&#39;", stdout);
			break;
		case '&':
			fputs("&amp;", stdout);
			break;
		default:
			fputc(*s, stdout);
		}
	}
}

static void
list(Info *info, char *dir, char *expr, char *html)
{
	Info tmp;
	InfoRow row;
	glob_t gl;
	char path[256], **pp, *sl;
	int pop;

	snprintf(path, sizeof path, "%s/%s", dir, expr);
	if(glob(path, 0, nil, &gl) != 0)
		goto End;

	for(pp = gl.gl_pathv; *pp; pp++){
		if(strcmp(sl = strrchr(*pp, '/'), "/info") == 0){
			if((info = inforead(info, *pp)) == nil)
				err(1, "parsing %s", *pp);
			*sl = '\0';
			pop = 1;
		}else{
			pop = 0;
		}

		row.key = "ref";
		row.val = *pp;
		tmp.vars = &row;
		tmp.next = info;
		tmp.len = 1;
		tmp.buf = nil;

		snprintf(path, sizeof path, "html/elem.%s.html", html);
		htmltemplate(path, &tmp);

		if(pop)
			info = infopop(info);
	}
End:
	globfree(&gl);
}

/*
 * extract parameters from head and bump tail pointer:
 *	text with {{parameter}} in some line
 *	          0 ^return  0 ^tail
 */
static char *
next(char *head, char **tail)
{
	char *beg, *end;

	if((beg = strstr(head, "{{")) == nil
	|| (end = strstr(beg, "}}")) == nil)
		return nil;
	*beg = *end = '\0';
	*tail = end + strlen("}}");
	return beg + strlen("{{");
}

/*
 * takes a list of (Info *) arguments by order of precedence
 */
void
htmltemplate(char *htmlpath, Info *info)
{
	FILE *fp = nil;
	size_t sz = 0;
	char *line = nil, *head, *tail, *param, *sp, *ref, *val;

	if((fp = fopen(htmlpath, "r")) == nil)
		err(1, "opening template %s", htmlpath);

	while(getline(&line, &sz, fp) > 0){
		head = tail = line;
		for(; (param = next(head, &tail)) != nil; head = tail){
			fputs(head, stdout);

			if(strcasecmp(param, "now") == 0){
				fprintf(stdout, "%lld", (long long)time(nil));

			}else if(strcasecmp(param, "nav") == 0){
				list(info, "data", "*/info", "nav");

			}else if((sp = strchr(param, ' '))){
				*sp = '\0';
				if((ref = infostr(info, "ref")) == nil)
					err(1, "no $ref");
				list(info, ref, sp + 1, param);

			}else if((val = infostr(info, param))){
				htmlprint(val);

			}else{
				fprintf(stdout, "{{error:%s}}", param);
			}
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}
