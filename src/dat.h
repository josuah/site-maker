#define CAT_MAX 1000
#define nil ((void *)0)

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
