#define CAT_MAX 1000
#define L(x) (sizeof(x) / sizeof(*(x)))

typedef long long vlong;
typedef size_t usize;
typedef struct Info Info;
typedef struct Irow Irow;

struct Irow
{
	char*	key;
	char*	val;
};

struct Info
{
	char*	buf;
	Irow*	vars;
	usize	len;
	Info*	next;
};

/* util */
extern char*	argv0;
extern void	sysfatal(char*, ...);
extern char*	strsep(char**, const char*);
extern vlong	strtonum(const char*, vlong, vlong, const char**);
extern char*	fopenread(char*);
extern char*	tr(char*, char*, char*);

/* cgi */
extern Info	headers[1];
extern Info	cookies[1];
extern Info*	cgiget(Info*);
extern Info*	cgipost(Info*);
extern void	cgifile(char*, usize);
extern Info*	cgienv(Info*);
extern Info*	cgicookies(Info*);
extern void	cgihead(char*);
extern void	cgifatal(char*, ...);
extern void	cgiredir(int, char*, ...);

/* info */
extern const char* infoerr;
extern void	infoadd(Info*, char*, char*);
extern void	infosort(Info*);
extern void	infofree(Info*);
extern Info*	infopop(Info*);
extern void	infoset(Info*, char*, char*);
extern char*	infostr(Info*, char*);
extern vlong	infonum(Info*, char*, vlong, vlong);
extern Info*	inforead(Info*, char*);
extern int	infowrite(Info*, char*);

/* html */
extern void	htmlprint(char*);
extern void	htmltemplate(char*, Info*);

/* fields */
extern char*	catfields[2];
