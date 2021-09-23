#define LEN(x) (sizeof(x) / sizeof(*(x)))

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
char *tr(char *, char const *, char const *);

/* cgi */
Info *cgiget(Info *);
Info *cgipost(Info *);
Info *cgienv(Info *);
void cgihead(void);
void cgierror(int, char *, ...);
void cgiredir(int, char *);

/* info */
extern char const *infoerr;
void infoadd(Info *, char *, char*);
void infosort(Info *);
void infofree(Info *);
Info *infopop(Info *);
void infoset(Info *, char *, char *);
char * infomiss(Info *, char **);
char *infostr(Info *, char *);
long long infonum(Info *, char *, long long, long long);
Info *inforead(Info *, char *);
int infowrite(Info *, char *);

/* html */
void htmlprint(char *);
void htmltemplate(char *, Info *);
