#include <stdlib.h>
#include <ctype.h>
#include "utils.h"

unsigned int utilsCalcHash(char *str)
{
	unsigned int hashval;

	for (hashval = 0; *str != '\0'; str++)
		hashval = *str + 31 * hashval;

	return hashval % HSIZE;
}

int utilsNameCMP(char *name1, char *name2)
{
	int num1, num2;
	char chr1, chr2;
	
	if (isdigit(*name1) && isdigit(*name2))
	{
		num1 = atof(name1);
		num2 = atof(name2);

		if (num1 < num2)
			return -1;
		else if (num1 > num2)
			return 1;
		else
			return 0;
	}

	do
	{
		chr1 = tolower(*name1++);
		chr2 = tolower(*name2++);
	
		if (chr1 == chr2 && chr1 == '\0')
			return 0;

	} while (chr1 == chr2);
	
	return chr1 - chr2;
}