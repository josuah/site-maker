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
	void (*fn)(int method, char const *path, char const *matches[]);
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
static void httpd_redir(int code, char *fmt, ...);


/// POLICE LINE /// DO NOT CROSS ///


struct httpd_var {
	char const *key, *val;
};

struct httpd_var_list {
	size_t len;
	struct httpd_var *list;
};

static char *httpd_path;
static struct httpd_var_list httpd_headers;
static struct httpd_var_list httpd_cookies;
static struct httpd_var_list httpd_payload;
static struct httpd_var_list httpd_parameters;
static int httpd_method;

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
httpd_parse_urlencoded(struct httpd_var_list *vars, char *s)
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
httpd_parse_query_string(void)
{
	if ((httpd_path = getenv("QUERY_STRING")) == NULL)
		httpd_fatal("no $QUERY_STRING");
	httpd_parse_urlencoded(&httpd_parameters, httpd_path);
}

static inline void
httpd_parse_payload(void)
{
	size_t len;
	char *buf, *env;

	if ((env = getenv("CONTENT_LENGTH")) == NULL)
		httpd_fatal("no $CONTENT_LENGTH");
	len = atoi(env);
	if ((buf = calloc(len + 1, 1)) == NULL)
		httpd_fatal("calloc");
	fread(buf, len, 1, stdin);
	if (ferror(stdin) || feof(stdin))
		httpd_fatal("reading POST data");
	if ((env = getenv("CONTENT_TYPE")) == NULL)
		httpd_fatal("no $Content-Type");
	if (strcasecmp(env, "application/x-www-form-urlencoded") != 0)
		httpd_fatal("expecting application/x-httpdw-form-urlencoded");
	httpd_parse_urlencoded(&httpd_payload, buf);
}

static inline void
httpd_parse_cookies(void)
{
	char *env, *val;

	if ((env = getenv("HTTP_COOKIE")) == NULL)
		return;
	while ((val = strsep(&env, ";"))) {
		val += (*val == ' ');
		httpd_add_var(&httpd_cookies, strsep(&val, "="), val);
	}
	httpd_sort_var_list(&httpd_cookies);
}

static inline void
httpd_parse_method(void)
{
	char *env;

	if ((env = getenv("REQUEST_METHOD")) == NULL)
		httpd_fatal("missing $REQUEST_METHOD");
	if (strcasecmp(env, "GET"))
		httpd_method = HTTPD_GET;
	else if (strcasecmp(env, "POST"))
		httpd_method = HTTPD_POST;
	else if (strcasecmp(env, "PUT"))
		httpd_method = HTTPD_PUT;
	else if (strcasecmp(env, "PATCH"))
		httpd_method = HTTPD_PATCH;
	else if (strcasecmp(env, "DELETE"))
		httpd_method = HTTPD_DELETE;
	else
		httpd_method = HTTPD_CUSTOM;
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

	if ((env = getenv("CONTENT_TYPE")) == NULL)
		httpd_fatal("no $Content-Type");

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
httpd_redir(int code, char *fmt, ...)
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
httpd_path_match(char const *match, char const *path)
{
	// TODO: '\1' and '**' and '/'
	while (*match != '*' && *path != '\0' && *match == *path)
		match++, path++;
	if (match[0] == '*') {
		if ((*match != '\0' && httpd_path_match(match + 1, path) == 0)
		 || (*path != '\0' && httpd_path_match(match, path + 1) == 0))
			return 0;
	}
	return (*match == '\0' && *path == '\0') ? 0 : -1;
}

static void
httpd_handle_request(struct httpd_handler h[])
{
	httpd_parse_query_string();
	httpd_parse_method();

	for (; h->match != NULL; h++) {
		if (httpd_path_match(h->match, httpd_path) == 0) {
			h->fn(httpd_method, httpd_path, NULL);
			return;
		}
	}
	httpd_fatal("no handler for '%s'", httpd_path);
}

#endif
