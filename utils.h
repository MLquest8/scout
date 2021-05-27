#define COLOR_DEFAULT -1
#define LIGHT(COLOR) COLOR + 8
#define ARRLENGTH(ARRAY)	(sizeof ARRAY / sizeof ARRAY[0])

#define HSIZE 101

unsigned int utilsCalcHash(char *);
int utilsNameCMP(char *, char *);