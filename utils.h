#define HSIZE 101
#define COLOR_DEFAULT -1
#define LIGHT(COLOR) COLOR + 8
#define ARRLENGTH(ARRAY) (sizeof ARRAY / sizeof ARRAY[0])

#define utilsFree(ptr) utilsFreeC((void *) &(ptr))

void utilsFreeC(void **);
void *utilsMalloc(size_t);
void *utilsCalloc(size_t, size_t);
void *utilsRealloc(void *, size_t);
unsigned int utilsCalcHash(char *);
int utilsNameCMP(char *, char *);