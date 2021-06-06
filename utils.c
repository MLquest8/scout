#include <stdlib.h>
#include <ctype.h>
#include "utils.h"

void utilsFreeC(void **ptr)
{
	if (ptr && *ptr)
	{
		free(*ptr);
		*ptr = NULL;
	}
}

void *utilsMalloc(size_t size)
{
	void *p;

	if (!(p = malloc(size)))
		exit(EXIT_FAILURE);

	return p;
}

void *utilsCalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		exit(EXIT_FAILURE);

	return p;
}

void *utilsRealloc(void *oldptr, size_t size)
{
	void *p;

	if (!(p = realloc(oldptr, size)))
		exit(EXIT_FAILURE);

	return p;
}

unsigned int utilsCalcHash(char *str)
{
	unsigned int hashval;

	for (hashval = 0; *str != '\0'; str++)
		hashval = *str + 31 * hashval;

	return hashval % HSIZE;
}

int utilsNameCMP(char *name1, char *name2)
{
	int chr1, chr2;
	long num1, num2;
	char *str1 = name1;
	char *str2 = name2;

	do
	{
		chr1 = tolower(*str1);
		chr2 = tolower(*str2);

		if (isdigit(chr1) && isdigit(chr2))
		{
			num1 = atol(str1);
			num2 = atol(str2);

			if (num1 < num2)
				return -1;
			else if (num1 > num2)
				return +1;
			else
			{
				while (isdigit(*str1++) && isdigit(*str2++));
				chr1 = tolower(*str1);
				chr2 = tolower(*str2);
			}
		}
	
		if (chr1 == chr2 && chr1 == '\0')
		{
			str1 = name1;
			str2 = name2;

			do
			{
				chr1 = *str1++;
				chr2 = *str2++;
			
				if (chr1 == chr2 && chr1 == '\0')
					return 0;

			} while (chr1 == chr2);
			
			return chr1 - chr2;
		}

		str1++;
		str2++;

	} while (chr1 == chr2);
	
	return chr1 - chr2;
}