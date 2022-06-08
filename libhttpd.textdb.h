
static inline char*
http_fopenread(char *path)
{
	FILE *fp;
	size_t sz;
	int c;
	char *buf;
	void *mem;

	buf = NULL;

	if ((fp = fopen(path, "r")) == NULL)
		return NULL;
	for (sz = 2;; sz++) {
		if ((mem = realloc(buf, sz)) == NULL)
			goto err;
		buf = mem;
		if ((c = fgetc(fp)) == EOF)
			break;
		buf[sz - 2] = c;
	}
	if (ferror(fp))
		goto err;
	buf[sz - 1] = '\0';
	return buf;
err:
	fclose(fp);
	free(buf);
	return NULL;
}

static struct http_var_list*
http_read_var_list(char *path)
{
	struct http_var_list *var;
	char *buf, *line, *tail, *key;

	buf = line = NULL;

	if ((var = calloc(sizeof *var, 1)) == NULL) {
		http_err = strerror(errno);
		goto err;
	}
	memset(var, 0, sizeof *var);

	tail = buf = http_fopenread(path);
	if (buf == NULL) {
		http_err = strerror(errno);
		goto err;
	}
	while ((line = http_strsep(&tail, "\n")) != NULL) {
		if (line[0] == '\0')
			break;
		key = http_strsep(&line, ":");
		if (line == NULL || *line++ != ' ') {
			http_err = "missing ': ' separator";
			goto err;
		}
		http_add_var(var, key, line);
	}
	http_add_var(var, "Text", tail ? tail : "");
	http_sort_var_list(var);

	return var;
err:
	free(buf);
	http_free_var(var);
	return NULL;
}

static int
http_write_var_list(struct http_var_list *var, char *dst)
{
	FILE *fp;
	HttpRow *row;
	size_t n;
	char path[1024], *text;

	text = NULL;

	snprintf(path, sizeof path, "%s.tmp", dst);
	if ((fp = fopen(path, "w")) == NULL) {
		http_err = "opening var file for writing";
		goto err;
	}

	for (row = vars->list, n = vars->len; n > 0; row++, n--) {
		if (strcasecmp(row->key, "Text") == 0) {
			text = text ? text : row->val;
			continue;
		}
		assert(strchr(row->key, '\n') == NULL);
		assert(strchr(row->val, '\n') == NULL);
		fprintf(fp, "%s: %s\n", row->key, row->val);
	}
	fprintf(fp, "\n%s", text ? text : "");

	fclose(fp);
	if (rename(path, dst) == -1) {
		http_err = "applying changes to var file";
		goto err;
	}
	return 0;
err:
	if (fp) fclose(fp);
	return -1;
}
