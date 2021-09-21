#define CAT_MAX 1000

typedef struct Info Info;
typedef struct InfoRow InfoRow;

struct InfoRow
{
	char *key, *val;
};

struct Info
{
	char *buf, *text;
	size_t len;
	int sorted;
	InfoRow *vars;
};
