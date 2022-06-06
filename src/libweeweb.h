#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <glob.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>


#define WEWE_HEX(x)(\
	((x) >= 'a' && (x) <= 'f') ? 10 + (x) - 'a' : \
	((x) >= 'A' && (x) <= 'F') ? 10 + (x) - 'A' : \
	((x) >= '0' && (x) <= '9') ? (x) - '0' : \
	0 \
)


typedef struct WeeDb WeeDb;
typedef struct WeeRow WeeRow;
typedef struct WeeFn WeeFn;


struct WeeRow {
	char *key;
	char *val;
};

struct WeeDb {
	char *buf;
	WeeRow *vars;
	size_t len;
	WeeDb *next;
};

struct WeeFn {
	char *name;
	void (*fn)(WeeDb *); 
};


WeeDb wee_headers[1], wee_cookies[1];
static char const *wee_err = "(no message)";


static inline char*
strsep(char **sp, char const *sep)
{
        char *s, *prev;

        if (*sp == NULL)
                return NULL;

        for (s = prev = *sp; strchr(sep, *s) == NULL; s++)
                continue;

        if (*s == '\0') {
                *sp = NULL;
        } else {
                *s = '\0';
                *sp = s + 1;
        }
        return prev;
}

static inline char*
wee_fopenread(char *path)
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
			goto Err;
		buf = mem;
		if ((c = fgetc(fp)) == EOF)
			break;
		buf[sz - 2] = c;
	}
	if (ferror(fp))
		goto Err;
	buf[sz - 1] = '\0';
	return buf;
Err:
	fclose(fp);
	free(buf);
	return NULL;
}


static inline void
wee_hex_decode(char *s)
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
			*w++ = (WEWE_HEX(s[1]) << 4) | WEWE_HEX(s[2]);
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

static inline WeeDb*
wee_parse_query(WeeDb *next, char *s)
{
	WeeDb *db;
	char *var, *eq;

	if ((db = calloc(sizeof *db, 1)) == NULL)
		wee_fatal("calloc");

	while ((var = strsep(&s, "&"))) {
		decode(var);
		if ((eq = strchr(var, '=')) == NULL)
			continue;
		*eq = '\0';
		wee_db_add(db, var, eq + 1);
	}
	wee_db_sort(db);
	db->next = next;
	return db;
}

WeeDb*
wee_cgi_get(WeeDb *next)
{
	char *query;

	if ((query = getenv("QUERY_STRING")) == NULL)
		wee_cgi_fatal("no $QUERY_STRING");
	return wee_parse_query(next, query);
}

WeeDb*
wee_cgi_post(WeeDb *next)
{
	WeeDb *db;
	size_t len;
	char *buf, *env;

	if ((env = getenv("CONTENT_LENGTH")) == NULL)
		wee_fatal("no $CONTENT_LENGTH");
	len = atoi(env);

	if ((buf = calloc(len + 1, 1)) == NULL)
		wee_fatal("calloc");

	fread(buf, len, 1, stdin);
	if (ferror(stdin) || feof(stdin))
		wee_cgi_fatal("reading POST data");

	if ((env = getenv("CONTENT_TYPE")) == NULL)
		wee_cgi_fatal("no $Content-Type");

	if (strcasecmp(env, "application/x-weew-form-urlencoded") != 0)
		wee_cgi_fatal("expecting application/x-weew-form-urlencoded");

	db = wee_parse_query(next, buf);
	db->buf = buf;
	return db;
}

WeeDb*
wee_cgi_cookies(WeeDb *next)
{
	WeeDb *db;
	char *env, *val;

	if ((db = calloc(sizeof *db, 1)) == NULL)
		wee_fatal("calloc");
	if ((env = getenv("HTTP_COOKIE")) == NULL)
		return db;
	while ((val = strsep(&env, ";"))) {
		val += (*val == ' ');
		wee_db_add(db, strsep(&val, "="), val);
	}
	wee_db_sort(db);
	db->next = next;
	return db;
}

static void
wee_cgi_file(char *path, size_t len)
{
	FILE *fp;
	pid_t pid;
	char *env, *s, *bound, *line;
	size_t sz, boundlen;
	int c;

	pid = getpid();
	line = NULL;
	sz = 0;

	snprintf(path, len, "tmp/%i", pid);
	if (mkdir(path, 0770) == -1)
		wee_cgi_fatal("making temporary directory %s", path);

	snprintf(path, len, "tmp/%i/file", pid);
	if ((fp = fopen(path, "w")) == NULL)
		wee_cgi_fatal("opening temporary upload file %s", path);

	if ((env = getenv("CONTENT_TYPE")) == NULL)
		wee_cgi_fatal("no $Content-Type");

	s = "multipart/form-data; boundary=";
	if (strncasecmp(env, s, strlen(s)) != 0)
		wee_cgi_fatal("expecting \"%s\"", s);
	bound = env + strlen(s);

	boundlen = strlen(bound);
	getline(&line, &sz, stdin);
	s = line;

	if (strncmp(s, "--", 2) != 0)
		wee_cgi_fatal("cannot parse form data: %s", line);
	s += 2;

	if (strncmp(s, bound, boundlen) != 0)
		wee_cgi_fatal("cannot recognise boundary");
	s += boundlen;
	s += (*s == '\r');

	if (*s != '\n')
		wee_cgi_fatal("expecting newline after boundary");
	s++;

	while (getline(&line, &sz, stdin) > 0 && line[*line == '\r'] != '\n');

	if (feof(stdin))
		wee_cgi_fatal("unexpected end-of-file before boundary end");

	while ((c = fgetc(stdin)) != EOF)
		fputc(c, fp);

	fflush(fp);
	if (ferror(stdin) || ferror(fp))
		wee_cgi_fatal("writing file to %s", path);

	ftruncate(fileno(fp), ftell(fp) - boundlen - strlen("----"));
}

static void
wee_cgi_head(char *type)
{
	WeeRow *row;
	size_t n;

	fprintf(stdout, "Content-Type: %s\n", type);

	for (row = headers->vars, n = headers->len; n > 0; n--, row++)
		if (strcasecmp(row->key, "text") != 0)
			fprintf(stdout, "%s: %s\n", row->key, row->val);

	for (row = cookies->vars, n = cookies->len; n > 0; n--, row++)
		fprintf(stdout, "Set-Cookie: %s=%s\n", row->key, row->val);

	fputc('\n', stdout);
}

static void
wee_cgi_fatal(char *fmt, ...)
{
	va_list va;
	char msg[1024];

	va_start(va, fmt);
        vsnprintf(msg, sizeof msg, fmt, va);
	va_end(va);
	fprintf(stdout, "Status: 500 %s\n", msg);
	fprintf(stdout, "Content-Type: text/plain\n");
	fprintf(stdout, "\n");
	fprintf(stdout, "Error: %s\n", msg);
	exit(0);
}

static void
wee_cgi_redir(int code, char *fmt, ...)
{
	va_list va;
	char url[1024];

	va_start(va, fmt);
        vsnprintf(url, sizeof url, fmt, va);
	va_end(va);
	fprintf(stdout, "Status: %i redirecting\n", code);
	fprintf(stdout, "Location: %s\n", url);
	wee_cgi_head("text/plain");
	fprintf(stdout, "Redirecting to %s\n", url);
}

/*
 * Key-value storage with chaining: queries for a key on WeeDb
 * fallback to WeeDb->next if key is not found.
 *
 * An RFC822-like format permits to store and read the data on a file:
 *
 *	First-Key-Name: One needle
 *	Second-Key-Name: Two haystacks
 *	Third-Key-Name: Three magnets
 * 
 *	Extra multiline text that will be read into the "Text" key.
 * 
 * This permits to mix key-values from various sources such as
 * environment variables, headers, cookies, get/post params, data
 * files...
 */

static int
cmp(const void *v1, const void *v2)
{
	return strcasecmp(((WeeRow *)v1)->key, ((WeeRow *)v2)->key);
}

static void
wee_db_sort(WeeDb *db)
{
	qsort(db->vars, db->len, sizeof *db->vars, cmp);
}

static void
wee_db_add(WeeDb *info, char *key, char *val)
{
	void *mem;

	info->len++;
	if ((mem = realloc(info->vars, info->len * sizeof *info->vars)) == NULL)
		 wee_fatal("realloc");
	info->vars = mem;
	info->vars[info->len-1].key = key;
	info->vars[info->len-1].val = val;
}

static char*
wee_db_str(WeeDb *info, char *key)
{
	WeeRow *r, q;

	q.key = key;

	if (info == NULL)
		return NULL;
	if ((r = bsearch(&q, info->vars, info->len, sizeof *info->vars, cmp)))
		return r->val;
	return wee_db_str(info->next, key);
}

static long long
wee_strtonum(char const *s, long long min, long long max, char const **errstr)
{
	long long ll;
	char *end;

	assert(min < max);

	errno = 0;
	ll = strtoll(s, &end, 10);
	if ((errno == ERANGE && ll == LLONG_MIN) || ll < min) {
		*errstr = "number too small";
		return 0;
	}
	if ((errno == ERANGE && ll == LLONG_MAX) || ll > max) {
		*errstr = "number too large";
		return 0;
	}
	if (errno == EINVAL || *end != '\0') {
		*errstr = "invalid number";
		return 0;
	}
	assert(errno == 0);
	*errstr = NULL;
	return ll;
}

static long long
wee_db_num(WeeDb *info, char *key, long long min, long long max)
{
	char *val;

	if ((val = wee_db_str(info, key)) == NULL)
		{ wee_err = "key not found"; goto Err; }
	return wee_strtonum(val, min, max, &wee_err);
Err:
	return 0;
}

/*
 * If key is already in info*, replace the value, otherwise append
 * it at the end.
 */
static void
wee_db_set(WeeDb *info, char *key, char *val)
{
	WeeRow *r, q;

	q.key = key;
	r = bsearch(&q, info->vars, info->len, sizeof *info->vars, cmp);
	if (r != NULL) {
		r->val = val;
		return;
	}
	wee_db_add(info, key, val);
	wee_db_sort(info);
}

WeeDb*
wee_db_pop(WeeDb *info)
{
	WeeDb *next;

	next = info->next;
	free(info->buf);
	free(info);
	return next;
}

static void
wee_db_free(WeeDb *info)
{
	while ((info = wee_db_pop(info)) != NULL);
}

WeeDb*
wee_db_read(WeeDb *next, char *path)
{
	WeeDb *info;
	char *buf, *line, *tail, *key;

	buf = line = NULL;

	if ((info = calloc(sizeof *info, 1)) == NULL) {
		wee_err = strerror(errno);
		goto Err;
	}
	memset(info, 0, sizeof *info);

	tail = buf = wee_fopenread(path);
	if (buf == NULL) {
		wee_err = strerror(errno);
		goto Err;
	}
	while ((line = strsep(&tail, "\n")) != NULL) {
		if (line[0] == '\0')
			break;
		key = strsep(&line, ":");
		if (line == NULL || *line++ != ' ')
			Err("missing ': ' separator");
		wee_db_add(info, key, line);
	}
	wee_db_add(info, "Text", tail ? tail : "");
	wee_db_sort(info);

	info->next = next;
	return info;
Err:
	free(buf);
	wee_db_free(info);
	return NULL;
}

int
wee_db_write(WeeDb *info, char *dst)
{
	FILE *fp;
	WeeRow *row;
	size_t n;
	char path[1024], *text;

	text = NULL;

	snprintf(path, sizeof path, "%s.tmp", dst);
	if ((fp = fopen(path, "w")) == NULL)
		Err("opening info file for writing")

	for (row = info->vars, n = info->len; n > 0; row++, n--) {
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
	if (rename(path, dst) == -1)
		Err("applying changes to info file");
	return 0;
Err:
	if (fp) fclose(fp);
	return -1;
}

static void
wee_html_print(char *s)
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

static WeeDb*
list_item(WeeDb *info, char *item, char *html)
{
	WeeDb tmp;
	WeeRow row;
	char path[256], *sl;
	int pop;

	sl = strrchr(item, '/');
	if (sl != NULL && strcmp(sl, "/info") == 0) {
		if ((info = wee_db_read(info, item)) == NULL)
			wee_fatal("parsing %s", item);
		*sl = '\0';
		pop = 1;
	} else {
		pop = 0;
	}

	row.key = "ref";
	row.val = strchr(item, '/');
	row.val = (row.val == NULL) ? "" : row.val + 1;

	tmp.vars = &row;
	tmp.next = info;
	tmp.len = 1;
	tmp.buf = NULL;

	snprintf(path, sizeof path, "html/elem.%s.html", html);
	wee_html_template(path, &tmp);

	return pop ? wee_db_pop(info) : info;
}

static void
list(WeeDb *info, char *dir, char *expr, char *html)
{
	glob_t gl;
	char path[256], **pp;
	int reverse = 0;

	if (*expr == '!') {
		reverse = 1;
		expr++;
	}

	if (*expr == '/')
		snprintf(path, sizeof path, "data%s", expr);
	else
		snprintf(path, sizeof path, "data/%s/%s", dir, expr);

	if (glob(path, 0, NULL, &gl) != 0)
		goto End;

	if (reverse) {
		for (pp = gl.gl_pathv; *pp; pp++);
		while (pp-- > gl.gl_pathv)
			info = list_item(info, *pp, html);
	} else {
		for (pp = gl.gl_pathv; *pp; pp++)
			info = list_item(info, *pp, html);
	}
End:
	globfree(&gl);
}

static void
cart(WeeDb *info)
{
	WeeDb *new;
	char *s, *ref, path[256];

	if ((s = wee_db_str(info, "cart")) == NULL) {
		wee_html_template("html/elem.cartempty.html", info);
		return;
	}
	while ((ref = strsep(&s, ","))) {
		snprintf(path, sizeof path, "%s/info", ref);
		if ((new = wee_db_read(info, path)) == NULL) {
			wee_html_template("html/elem.cartmissing.html", info);
			continue;
		}
		wee_db_set(new, "ref", ref);
		wee_html_template("html/elem.cart.html", new);
		wee_db_pop(new);
	}
}

static void
cart_count(WeeDb *info)
{
	char *cart;
	size_t count;

	count = 0;
	if ((cart = wee_db_str(info, "cart"))) {
		count += (*cart != '\0');
		while ((cart = strchr(cart, ',')))
			cart++, count++;
	}
	fprintf(stdout, "%zu", count);
}

static void
date(WeeDb *info)
{
	char buf[256];
	time_t clock;

	clock = time(NULL);
	strftime(buf, sizeof buf, "%Y-%m-%d", localtime(&clock));
	fputs(buf, stdout);
}

static void
now(WeeDb *info)
{
	fprintf(stdout, "%lld", (long long)time(NULL));
}

static void
parent(WeeDb *info)
{
	char buf[256], *ref, *sl;

	if ((ref = wee_db_str(info, "ref")) == NULL)
		wee_fatal("no $ref");
	strlcpy(buf, ref, sizeof buf);
	if ((sl = strrchr(buf, '/'))) {
		*sl = '\0';
		fputs(buf, stdout);
	}
}

static void
strip_1(WeeDb *info)
{
	char *ref, *new;

	if ((ref = wee_db_str(info, "ref")) == NULL)
		wee_fatal("no $ref");
	new = strchr(ref, '/');
	fputs((new == NULL) ? "" : new + 1, stdout);
}

#define F(name) { #name, name }
static WeeFn wee_html_fn[] = {
	F(cart), F(cart_count), F(date), F(now), F(parent), F(strip_1)
};
#undef F

static int
wee_html_cmp(const void *v1, const void *v2)
{
	return strcasecmp(((WeeFn *)v1)->name, ((WeeFn *)v2)->name);
}

static char*
next(char *head, char **tail)
{
	char *beg, *end;

	if ((beg = strstr(head, "{{")) == NULL
	|| (end = strstr(beg, "}}")) == NULL)
		return NULL;
	*beg = *end = '\0';
	*tail = end + strlen("}}");
	return beg + strlen("{{");
}

/*
 * takes a list of WeeDb * arguments by order of precedence
 */
static void
wee_html_template(char *wee_html_path, WeeDb *info)
{
	FILE *fp;
	WeeFn *f, q;
	size_t sz;
	char *line, *head, *tail, *sp, *ref, *val;

	sz = 0;
	line = NULL;

	if ((fp = fopen(wee_html_path, "r")) == NULL)
		wee_fatal("opening template %s", wee_html_path);

	while (getline(&line, &sz, fp) > 0) {
		head = tail = line;
		for (; (q.name = next(head, &tail)); head = tail) {
			fputs(head, stdout);
			f = bsearch(&q, wee_html_fn,
			  sizeof wee_html_fn / sizeof *wee_html_fn,
			  sizeof *wee_html_fn, wee_html_cmp);
			if (f != NULL) {
				f->fn(info);

			} else if ((sp = strchr(q.name, ' '))) {
				*sp = '\0';
				if ((ref = wee_db_str(info, "ref")) == NULL)
					wee_fatal("no $ref");
				list(info, ref, sp + 1, q.name);

			} else if ((val = wee_db_str(info, q.name))) {
				wee_html_print(val);

			} else {
				fprintf(stdout, "{{error:%s}}", q.name);
			}
		}
		fputs(tail, stdout);
	}
	fclose(fp);
}
