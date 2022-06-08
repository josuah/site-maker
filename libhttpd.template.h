	snprintf(path, sizeof path, "tmp/%i", getpid());

static void
template_print_html(char *s)
{
	for (; *s != '\0'; s++) {
		switch(*s) {
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

static inline char*
template_next_var(char *head, char **tail)
{
	char *beg, *end;

	if ((beg = strstr(head, "{{")) == NULL
	  || (end = strstr(beg, "}}")) == NULL)
		return NULL;
	*beg = *end = '\0';
	*tail = end + strlen("}}");
	return beg + strlen("{{");
}

static void
template(char *path, struct http_var_list *vars)
{
	FILE *fp;
	HttpFn *f, q;
	size_t sz;
	char *line, *head, *tail, *sp, *ref, *val;

	sz = 0;
	line = NULL;

	if ((fp = fopen(path, "r")) == NULL)
		http_fatal("opening template %s", path);

	while (getline(&line, &sz, fp) > 0) {
		head = tail = line;
		for (; (q.name = http_next_var(head, &tail)); head = tail) {
			fputs(head, stdout);
			f = bsearch(&q, http_html_fn,
			  sizeof http_html_fn / sizeof *http_html_fn,
			  sizeof *http_html_fn, http_html_cmp);
			if (f != NULL) {
				f->fn(vars);

			} else if ((sp = strchr(q.name, ' '))) {
				*sp = '\0';
				if ((ref = http_var_get(vars, "ref")) == NULL)
					http_fatal("no $ref");
				http_var_search(vars, ref, sp + 1, q.name);

			} else if ((val = http_var_get(vars, q.name))) {
				http_html_print(val);

			} else {
				fprintf(stdout, "{{error:%s}}", q.name);
			}
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}
