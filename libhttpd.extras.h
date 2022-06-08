static inline char *
httpd_fopenread(char *path)
{
	char *buf;
	size_t sz;

	if ((fp = fopen(path, "r")) == NULL)
		return NULL;
	fseek(fp, 0, SEEK_END):
	sz = ftell(fp);
	fseek(fp, 0, SEEK_BEG);
	if ((buf = malloc(sz + 1)) == NULL)
		return NULL;
	fread(buf, sz, 1, fp);
	fclose(fp);
	buf[sz] = '\0';
	return ferror(fp) ? NULL : buf;
}

static void
httpd_read_var_list(struct httpd_var_list *vars, char *path)
{
	struct httpd_var_list *vars;
	char *line, *tail, *key;

	buf = line = NULL;

	if ((tail = vars->buf = httpd_fopenread(path)) == NULL)
		httpd_fatal("opening %s: %s", path, strerror(errno));
	while ((line = httpd_strsep(&tail, "\n")) != NULL) {
		if (line[0] == '\0')
			break;
		key = httpd_strsep(&line, ":");
		if (line == NULL || *line++ != ' ')
			httpd_fatal("%s: missing ': ' separator", path);
		httpd_add_var(vars, key, line);
	}
	httpd_add_var(vars, "Text", tail ? tail : "");
	httpd_sort_var_list(vars);
}

static int
httpd_write_var_list(struct httpd_var_list *vars, char *dst)
{
	FILE *fp;
	struct httpd_var *v;
	size_t n;
	char path[1024], *text;

	text = NULL;

	snprintf(path, sizeof path, "%s.tmp", dst);
	if ((fp = fopen(path, "w")) == NULL)
		httpd_fatal("opening '%s' for writing", path);

	for (v = vars->list, n = vars->len; n > 0; v++, n--) {
		if (strcasecmp(v->key, "Text") == 0) {
			text = text ? text : v->val;
			continue;
		}
		assert(strchr(v->key, '\n') == NULL);
		assert(strchr(v->val, '\n') == NULL);
		fprintf(fp, "%s: %s\n", v->key, v->val);
	}
	fprintf(fp, "\n%s", text ? text : "");

	fclose(fp);
	if (rename(path, dst) == -1)
		httpd_fatal( "renaming '%s' to '%s'", path, dst);
	return 0;
}

#endif
