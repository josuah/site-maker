#ifndef LIBHTTPD_H
#define LIBHTTPD_H

/* HTTP server library to use in CGI scripts */

enum {
	HTTPD_GET	= 1 << 0,
	HTTPD_POST	= 1 << 1,
	HTTPD_PUT	= 1 << 2,
	HTTPD_PATCH	= 1 << 3,
	HTTPD_DELETE	= 1 << 4,
	HTTPD_CUSTOM	= 1 << 5,
	HTTPD_ANY	= 1 << 6,
};

/* map of glob-like patterns and handlers triggered by them */
static struct httpd_handler {
	char const *match;
	void (*fn)(int method, char **matches);
} *httpd_handlers;

/* main loop executing h->fn() if h->match is matching */
static void httpd_handle_request(struct httpd_handler h[]);

/* abort the program with an error message sent to the client */
static void httpd_fatal(char *fmt, ...);

/* receive a file payload from the client onto the disk at `path` */
static void httpd_receive_file(char const *path);

/* receive and parse the url-encoded HTTP payload instead of a file */
static void httpd_receive_payload(void);

/* set the response header `key` to be `val` */
static void httpd_set_header(char const *key, char const *val);

/* send the HTTP and CGI headers to the client, with Content-Type set to `type` */
static void httpd_send_headers(int code, char const *type);

/* send redirection headers with an HTTP `code` and a custom message `fmt` */
static void httpd_redirect(int code, char *fmt, ...);


/// POLICE LINE /// DO NOT CROSS ///


#define HTTPD_MATCH_NUM 5

struct httpd_var {
	char const *key, *val;
};

struct httpd_var_list {
	size_t len;
	struct httpd_var *list;
};

static char *httpd_path;
static struct httpd_var_list httpd_headers;
static struct httpd_var_list httpd_payload;
static struct httpd_var_list httpd_query_string;
static struct httpd_var_list httpd_cookies;

static void
httpd_fatal(char *fmt, ...)
{
	va_list va;
	char msg[1024];

	va_start(va, fmt);
	vsnprintf(msg, sizeof msg, fmt, va);
	va_end(va);
	fprintf(stdout, "Status: 500 %s\n", msg);
	fprintf(stdout, "Content-Type: text/plain\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "error: %s\n", msg);
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

static int
httpd_cmp_var(const void *v1, const void *v2)
{
	return strcasecmp(((struct httpd_var *)v1)->key, ((struct httpd_var *)v2)->key);
}

static inline void
httpd_add_var(struct httpd_var_list *vars, char const *key, char const *val)
{
	void *mem;

	vars->len++;
	if ((mem = realloc(vars->list, vars->len * sizeof *vars->list)) == NULL)
		 httpd_fatal("realloc");
	vars->list = mem;
	vars->list[vars->len-1].key = key;
	vars->list[vars->len-1].val = val;
}

static inline char const *
httpd_get_var(struct httpd_var_list *vars, char *key)
{
	struct httpd_var *r, q;

	q.key = key;
	if ((r = bsearch(&q, vars->list, vars->len, sizeof *vars->list, httpd_cmp_var)))
		return r->val;
	return NULL;
}

static inline void
httpd_sort_var_list(struct httpd_var_list *vars)
{
	qsort(vars->list, vars->len, sizeof *vars->list, httpd_cmp_var);
}

static inline void
httpd_set_var(struct httpd_var_list *vars, char const *key, char const *val)
{
	struct httpd_var *r, q;

	q.key = key;
	r = bsearch(&q, vars->list, vars->len, sizeof *vars->list, httpd_cmp_var);
	if (r != NULL) {
		r->val = val;
		return;
	}
	httpd_add_var(vars, key, val);
	httpd_sort_var_list(vars);
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

static inline void
httpd_receive_payload(void)
{
	size_t len;
	char *buf;

	len = atoi(httpd_get_env("CONTENT_LENGTH"));
	if ((buf = calloc(len + 1, 1)) == NULL)
		httpd_fatal("calloc");
	fread(buf, len, 1, stdin);
	if (ferror(stdin) || feof(stdin))
		httpd_fatal("reading POST data");
	if (strcasecmp(httpd_get_env("CONTENT_TYPE"), "application/x-www-form-urlencoded") != 0)
		httpd_fatal("expecting application/x-httpdw-form-urlencoded");
	httpd_decode_url(&httpd_payload, buf);
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
		httpd_fatal("expecting \"%s\"", s);
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
httpd_set_header(char const *key, char const *val)
{
	httpd_set_var(&httpd_headers, key, val);
	httpd_sort_var_list(&httpd_headers);
}

static void
httpd_add_header(char const *key, char const *val)
{
	httpd_add_var(&httpd_headers, key, val);
	httpd_sort_var_list(&httpd_headers);
}

static void
httpd_send_headers(int code, char const *type)
{
	struct httpd_var *v;
	size_t n;

	fprintf(stdout, "Status: %i\n", code);
	fprintf(stdout, "Content-Type: %s\n", type);
	for (v = httpd_headers.list, n = httpd_headers.len; n > 0; n--, v++)
		if (strcasecmp(v->key, "text") != 0)
			fprintf(stdout, "%s: %s\n", v->key, v->val);
	for (v = httpd_cookies.list, n = httpd_cookies.len; n > 0; n--, v++)
		fprintf(stdout, "Set-Cookie: %s=%s\n", v->key, v->val);
	fputc('\n', stdout);
}

static void
httpd_redirect(int code, char *fmt, ...)
{
	va_list va;
	char url[1024];

	va_start(va, fmt);
	vsnprintf(url, sizeof url, fmt, va);
	va_end(va);
	fprintf(stdout, "Location: %s\n", url);
	httpd_send_headers(code, "text/plain");
	fprintf(stdout, "Redirecting to %s\n", url);
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
		if (*glob != '\0' && httpd_match(glob + 1, path, matches, m + 1) >= 0) {
			if (matches[m] == NULL)
				matches[m] = path;
			*path = '\0';
			return m;
		} else if (*path != '\0' && httpd_match(glob, path + 1, matches, m) >= 0) {
			matches[m] = (char *)path;
			return m;
		}
	}
	return (*glob == '\0' && *path == '\0') ? m : -1;
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
		{ NULL,		HTTPD_CUSTOM }
	}, *m;

	env = httpd_get_env("REQUEST_METHOD");
	for (m = map; m->name != NULL && strcasecmp(env, m->name) != 0; m++);
	return m->val;
}

static inline void
httpd_parse_cookies(void)
{
	char *env;

	if ((env = getenv("HTTP_COOKIE")) == NULL)
		return;
	for (char *val; (val = strsep(&env, ";"));) {
		val += (*val == ' ');
		httpd_add_var(&httpd_cookies, strsep(&val, "="), val);
	}
	httpd_sort_var_list(&httpd_cookies);
}

static inline void
httpd_parse_query_string(void)
{
	httpd_decode_url(&httpd_query_string, httpd_get_env("QUERY_STRING"));
}

static void
httpd_handle_request(struct httpd_handler h[])
{
	char *path;
	int method;
	size_t len;

	path = httpd_get_env("PATH_INFO");
	method = httpd_parse_method();
	httpd_parse_cookies();
	httpd_parse_query_string();

	len = strlen(path);
	if (len == 0 || path[len - 1] != '/') {
		httpd_redirect(301, "%s/", path);
		return;
	}

	for (; h->match != NULL; h++) {
		char *matches[HTTPD_MATCH_NUM + 1];

		if (httpd_match(h->match, path, matches, 0) == 0) {
			h->fn(method, matches);
			return;
		}
	}
	httpd_fatal("no handler for '%s'", httpd_path);
}

#endif
