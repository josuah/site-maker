#define CAT_MAX 1000
#define nil ((void *)0)
#define L(x) (sizeof(x) / sizeof(*(x)))

typedef struct Info Info;
typedef struct InfoRow InfoRow;

struct InfoRow
{
	char *key, *val;
};

struct Info
{
	char *buf;

	InfoRow *vars;
	size_t len;

	Info *next;
};

/* util */
extern char *arg0;
void sysfatal(char const *, ...);
char *strsep(char **, char const *);
long long strtonum(char const *, long long, long long, char const **);
char *fopenread(char const *);
void isort(void *, size_t, size_t, int (*)(void const *, void const *));
char *tr(char *, char const *, char const *);

/* cgi */
extern Info headers[1], cookies[1];
Info *cgiget(Info *);
Info *cgipost(Info *);
void cgifile(char *, size_t);
Info *cgienv(Info *);
Info *cgicookies(Info *);
void cgihead(char *);
void cgifatal(char *, ...);
void cgiredir(int, char *, ...);

/* info */
extern char const *infoerr;
void infoadd(Info *, char *, char*);
void infosort(Info *);
void infofree(Info *);
Info *infopop(Info *);
void infoset(Info *, char *, char *);
char *infostr(Info *, char *);
long long infonum(Info *, char *, long long, long long);
Info *inforead(Info *, char *);
int infowrite(Info *, char *);

/* html */
void htmlprint(char *);
void htmltemplate(char *, Info *);

/* fields */
extern char *catfields[2];
