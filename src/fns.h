/* html */
void htmlprint(char *);
void htmlcat(char *);
void htmlcategories(void);

/* util */
extern char *arg0;
void err(int, char const *, ...);
void errx(int, char const *, ...);
void warn(char const *, ...);
void warnx(char const *, ...);
