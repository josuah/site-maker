#define LEN(x) (sizeof(x) / sizeof(*(x)))

/* html */
void htmlprint(char *);
void htmltemplate(char *, Info *);

/* cgi */
extern char const *cgierror;
char *cgiquery(char const *);
long long cgiquerynum(char const *, long long, long long, char const **);
void cgihead(void);

/* util */
extern char *arg0;
void err(int, char const *, ...);
void errx(int, char const *, ...);
void warn(char const *, ...);
void warnx(char const *, ...);
char *strsep(char **, char const *);
long long strtonum(char const *, long long, long long, char const **);
char *fopenread(char const *);
void isort(void *, size_t, size_t, int (*)(void const *, void const *));

/* info */
Info *infonew(Info *, char *);
void infofree(Info *);
Info *infopop(Info *);
int infoset(Info *, char *, char *);
char *infoget(Info *, char *);
Info *infodefault(void);
