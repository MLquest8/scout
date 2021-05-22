#define MAXBUF 999
#define COLOR_DEFAULT -1
#define LIGHT(COLOR) COLOR + 8
#define ARRLENGTH(ARRAY)	(sizeof ARRAY / sizeof ARRAY[0])

int utilsNameCMP(char *, char *);
unsigned int utilsGenerateHash(char *);