#ifndef LIBHTTPD_H
#define LIBHTTPD_H

/* HTTP server library to use in CGI scripts */

enum {
	HTTPD_GET	= 1 << 0,
	HTTPD_POST	= 1 << 1,
	HTTPD_PUT	= 1 << 2,
	HTTPD_PATCH	= 1 << 3,
	HTTPD_DELETE	= 1 << 4,
	HTTPD_HEAD	= 1 << 5,
	HTTPD_CUSTOM	= 1 << 6,
	HTTPD_ANY	= 0xFF,
};

/* maps glob pattern */
struct httpd_handler {
	int method;
	char const *glob;
	void (*fn)(char **matches);
};

/* storage for key-value pair */
struct httpd_var_list {
	struct httpd_var {
		char *key, *val;
	} *list;
	size_t len;
	char *buf;
};

/* main loop executing h->fn() if h->glob is matching */
static void httpd_handle_request(struct httpd_handler h[]);

/* abort the program with an error message sent to the client */
static void httpd_fatal(char *fmt, ...);

/* receive a file payload from the client onto the disk at `path` */
static void httpd_receive_file(char const *path);

/* send the HTTP and CGI headers to the client, with Content-Type set to `type` */
static void httpd_send_headers(int code, char const *type);

/* send redirection headers with an HTTP `code` and a custom message `fmt` */
static void httpd_redirect(int code, char *fmt, ...);

/* print a template with every "{{name}}" looked up in `vars` */
static void httpd_template(char const *path, struct httpd_var_list *vars);

/* print `s` with all html-special characters escaped */
static void httpd_print_html(char const *s);

/* manage a `key`-`val` pair storage `vars`, as used with httpd_template */
static void httpd_add_var(struct httpd_var_list *vars, char *key, char *val);
static void httpd_sort_var_list(struct httpd_var_list *vars);
static void httpd_set_var(struct httpd_var_list *vars, char *key, char *val);
static char *httpd_get_var(struct httpd_var_list *vars, char *key);
static void httpd_free_var_list(struct httpd_var_list *vars);

/* store and read a list of variables onto a simple RFC822-like format */
static void httpd_read_var_list(struct httpd_var_list *vars, char *path);
static int httpd_write_var_list(struct httpd_var_list *vars, char *path);

/* parse various components of the HTTP request */
static struct httpd_var_list * httpd_parse_payload(void);
static struct httpd_var_list * httpd_parse_cookies(void);
static struct httpd_var_list * httpd_parse_query_string(void);

/* sent to the client by httpd_send_headers */
static struct httpd_var_list httpd_headers;
static struct httpd_var_list httpd_cookies;


/// POLICE LINE /// DO NOT CROSS ///


#define HTTPD_MATCH_NUM 5

static void
httpd_fatal(char *fmt, ...)
{
	va_list va;
	char msg[1024];

	va_start(va, fmt);
	vsnprintf(msg, sizeof msg, fmt, va);
	printf("Status: 500 Server Error\n\n");
	printf("error: %s\n", msg);
	exit(1);
}

static inline char *
httpd_get_env(char const *key)
{
	char *env;

	if ((env = getenv(key)) == NULL)
		httpd_fatal("no $%s", key);
	return env;
}

static inline char *
httpd_fopenread(char *path)
{
	FILE *fp;
	char *buf;
	size_t sz;

	if ((fp = fopen(path, "r")) == NULL)
		return NULL;
	fseek(fp, 0, SEEK_END);
	sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if ((buf = malloc(sz + 1)) == NULL)
		return NULL;
	fread(buf, sz, 1, fp);
	fclose(fp);
	buf[sz] = '\0';
	return ferror(fp) ? NULL : buf;
}

static int
httpd_cmp_var(const void *v1, const void *v2)
{
	return strcasecmp(((struct httpd_var *)v1)->key, ((struct httpd_var *)v2)->key);
}

static void
httpd_add_var(struct httpd_var_list *vars, char *key, char *val)
{
	void *mem;

	vars->len++;
	if ((mem = realloc(vars->list, vars->len * sizeof *vars->list)) == NULL)
		 httpd_fatal("realloc");
	vars->list = mem;
	vars->list[vars->len-1].key = key;
	vars->list[vars->len-1].val = val;
}

static void
httpd_sort_var_list(struct httpd_var_list *vars)
{
	qsort(vars->list, vars->len, sizeof *vars->list, httpd_cmp_var);
}

static char *
httpd_get_var(struct httpd_var_list *vars, char *key)
{
	struct httpd_var *v, q = { .key = key };

	v = bsearch(&q, vars->list, vars->len, sizeof *vars->list, httpd_cmp_var);
	return (v == NULL) ? NULL : v->val;
}

static void
httpd_set_var(struct httpd_var_list *vars, char *key, char *val)
{
	struct httpd_var *v, q;

	q.key = key;
	v = bsearch(&q, vars->list, vars->len, sizeof *vars->list, httpd_cmp_var);
	if (v != NULL) {
		v->val = val;
		return;
	}
	httpd_add_var(vars, key, val);
	httpd_sort_var_list(vars);
}

static void
httpd_read_var_list(struct httpd_var_list *vars, char *path)
{
	char *line, *tail, *key, *s;

	line = NULL;

	if ((tail = vars->buf = httpd_fopenread(path)) == NULL)
		httpd_fatal("opening %s: %s", path, strerror(errno));
	while ((line = strsep(&tail, "\n")) != NULL) {
		if (line[0] == '\0')
			break;
		key = strsep(&line, ":");
		if (line == NULL || *line++ != ' ')
			httpd_fatal("%s: missing ': ' separator", path);
		httpd_add_var(vars, key, line);
	}
	httpd_set_var(vars, "text", tail ? tail : "");
	httpd_set_var(vars, "file", (s = strrchr(path, '/')) ? s + 1 : path);
	httpd_sort_var_list(vars);
}

static void
httpd_free_var_list(struct httpd_var_list *vars)
{
	if (vars->buf != NULL)
		free(vars->buf);
	free(vars->list);
}

static int
httpd_write_var_list(struct httpd_var_list *vars, char *dst)
{
	FILE *fp;
	struct httpd_var *v;
	size_t n;
	char path[1024];
	char *text;

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

static inline void
httpd_decode_hex(char *s)
{
	char *w;

	for (w = s;;) {
		switch(*s) {
		case '+':
			*w++ = ' ';
			s++;
			break;
		case '%':
			if (!isxdigit(s[1]) || !isxdigit(s[2])) {
				s++;
				continue;
			}
#define HEX(x)(\
	((x) >= 'a' && (x) <= 'f') ? 10 + (x) - 'a' : \
	((x) >= 'A' && (x) <= 'F') ? 10 + (x) - 'A' : \
	((x) >= '0' && (x) <= '9') ? (x) - '0' : \
	0 \
)
			*w++ = (HEX(s[1]) << 4) | HEX(s[2]);
#undef HEX
			s += 3;
			break;
		case '\0':
			*w = '\0';
			return;
		default:
			*w++ = *s;
			s++;
		}
	}
}

static inline void
httpd_decode_url(struct httpd_var_list *vars, char *s)
{
	char *tok, *eq;

	while ((tok = strsep(&s, "&"))) {
		httpd_decode_hex(tok);
		if ((eq = strchr(tok, '=')) == NULL)
			continue;
		*eq = '\0';
		httpd_add_var(vars, tok, eq + 1);
	}
	httpd_sort_var_list(vars);
}

static void
httpd_receive_file(char const *path)
{
	FILE *fp;
	char *env, *s, *bound, *line;
	size_t sz, boundlen;
	int c;

	line = NULL;
	sz = 0;

	env = httpd_get_env("CONTENT_TYPE");
	s = "multipart/form-data; boundary=";
	if (strncasecmp(env, s, strlen(s)) != 0)
		httpd_fatal("expecting '%s'", s);
	bound = env + strlen(s);

	if ((fp = fopen(path, "wx")) == NULL)
		httpd_fatal("opening temporary upload file %s", path);

	boundlen = strlen(bound);
	getline(&line, &sz, stdin);
	s = line;

	if (strncmp(s, "--", 2) != 0)
		httpd_fatal("cannot parse form data: %s", line);
	s += 2;

	if (strncmp(s, bound, boundlen) != 0)
		httpd_fatal("cannot recognise boundary");
	s += boundlen;
	s += (*s == '\r');

	if (*s != '\n')
		httpd_fatal("expecting newline after boundary");
	s++;

	while (getline(&line, &sz, stdin) > 0 && line[*line == '\r'] != '\n');

	if (feof(stdin))
		httpd_fatal("unexpected end-of-file before boundary end");

	while ((c = fgetc(stdin)) != EOF)
		fputc(c, fp);

	fflush(fp);
	if (ferror(stdin) || ferror(fp))
		httpd_fatal("writing file to %s", path);

	ftruncate(fileno(fp), ftell(fp) - boundlen - strlen("----"));
}

static void
httpd_send_headers(int code, char const *type)
{
	struct httpd_var *v;
	size_t n;

	printf("Status: %i\n", code);
	printf("Content-Type: %s\n", type);
	for (v = httpd_headers.list, n = httpd_headers.len; n > 0; n--, v++)
		if (strcasecmp(v->key, "text") != 0)
			printf("%s: %s\n", v->key, v->val);
	for (v = httpd_cookies.list, n = httpd_cookies.len; n > 0; n--, v++)
		printf("Set-Cookie: %s=%s; Path=/\n", v->key, v->val);
	fputc('\n', stdout);
}

static void
httpd_redirect(int code, char *fmt, ...)
{
	va_list va;
	char url[1024];

	va_start(va, fmt);
	vsnprintf(url, sizeof url, fmt, va);
	printf("Location: %s\n", url);
	httpd_send_headers(code, "text/plain");
	printf("Redirecting to %s\n", url);
}

static inline int
httpd_match(char const *glob, char *path, char **matches, size_t m)
{
	if (m >= HTTPD_MATCH_NUM)
		httpd_fatal("too many wildcards in glob");
	matches[m] = NULL;
	while (*glob != '*' && *path != '\0' && *glob == *path)
		glob++, path++;
	if (glob[0] == '*') {
		if (*glob != '\0' && httpd_match(glob + 1, path, matches, m + 1)) {
			if (matches[m] == NULL)
				matches[m] = path;
			*path = '\0';
			return 1;
		} else if (*path != '\0' && httpd_match(glob, path + 1, matches, m)) {
			matches[m] = (char *)path;
			return 1;
		}
	}
	return *glob == '\0' && *path == '\0';
}

static inline int
httpd_parse_method(void)
{
	char *env;
	static struct {
		char const *name;
		int val;
	} map[] = {
		{ "GET",	HTTPD_GET },
		{ "POST",	HTTPD_POST },
		{ "PUT",	HTTPD_PUT },
		{ "PATCH",	HTTPD_PATCH },
		{ "DELETE",	HTTPD_DELETE },
		{ "HEAD",	HTTPD_HEAD },
		{ NULL,		HTTPD_CUSTOM }
	}, *m;

	env = httpd_get_env("REQUEST_METHOD");
	for (m = map; m->name != NULL && strcasecmp(env, m->name) != 0; m++);
	return m->val;
}

static struct httpd_var_list *
httpd_parse_payload(void)
{
	static struct httpd_var_list vars = {0};
	size_t len;
	char *buf;

	if (vars.len > 0)
		return &vars;

	len = atoi(httpd_get_env("CONTENT_LENGTH"));
	if ((buf = calloc(len + 1, 1)) == NULL)
		httpd_fatal("calloc");
	fread(buf, len, 1, stdin);
	if (ferror(stdin) || feof(stdin))
		httpd_fatal("reading POST data");
	if (strcasecmp(httpd_get_env("CONTENT_TYPE"), "application/x-www-form-urlencoded") != 0)
		httpd_fatal("expecting application/x-www-form-urlencoded");
	httpd_decode_url(&vars, buf);
	vars.buf = buf;

	return &vars;
}

static struct httpd_var_list *
httpd_parse_cookies(void)
{
	static struct httpd_var_list vars = {0};
	char *env;

	if (vars.len > 0)
		return &vars;

	if ((env = getenv("HTTP_COOKIE")) == NULL)
		return &vars;
	for (char *val; (val = strsep(&env, ";"));) {
		val += (*val == ' ');
		httpd_add_var(&vars, strsep(&val, "="), val);
	}
	httpd_sort_var_list(&vars);

	return &vars;
}

static struct httpd_var_list *
httpd_parse_query_string(void)
{
	static struct httpd_var_list vars = {0};

	if (vars.len > 0)
		return &vars;

	httpd_decode_url(&vars, httpd_get_env("QUERY_STRING"));

	return &vars;
}

static void
httpd_handle_request(struct httpd_handler h[])
{
	char *path;
	int method;
	size_t len;

	path = httpd_get_env("PATH_INFO");
	method = httpd_parse_method();

	len = strlen(path);
	if (len == 0 || path[len - 1] != '/') {
		httpd_redirect(301, "%s/", path);
		return;
	}

	for (; h->glob != NULL; h++) {
		char *matches[HTTPD_MATCH_NUM + 1];
		if (!(h->method & method))
			continue;
		if (!httpd_match(h->glob, path, matches, 0))
			continue;
		h->fn(matches);
		return;
	}
	httpd_fatal("no handler for '%s'", path);
}

static void
httpd_print_html(char const *s)
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
httpd_next_var(char *head, char **tail)
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
httpd_template(char const *path, struct httpd_var_list *vars)
{
	FILE *fp;
	size_t sz;
	char *line, *head, *tail, *key;
	char *val;

	sz = 0;
	line = NULL;

	if ((fp = fopen(path, "r")) == NULL)
		httpd_fatal("opening template %s", path);

	while (getline(&line, &sz, fp) > 0) {
		head = tail = line;
		for (; (key = httpd_next_var(head, &tail)); head = tail) {
			fputs(head, stdout);
			if ((val = httpd_get_var(vars, key)))
				httpd_print_html(val);
			else
				fprintf(stdout, "{{error:%s}}", key);
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}

#endif
