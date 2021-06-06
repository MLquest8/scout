#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include "utils.h"

static FILE *logfile;

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
		utilsLogCommit(FATAL, "Malloc failure: size(%ul)", size);

	return p;
}

void *utilsCalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		utilsLogCommit(FATAL, "Calloc failure: nmemb(%ul) size(%ul)", nmemb, size);

	return p;
}

void *utilsRealloc(void *oldptr, size_t size)
{
	void *p;

	if (!(p = realloc(oldptr, size)))
		utilsLogCommit(FATAL, "Realloc failure: oldptr(%p) size(%ul)", oldptr, size);

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
				for (; isdigit(*str1) && isdigit(*str2); str1++, str2++);
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

void utilsLogBegin(const char *file)
{
	time_t curtime;

	if ((logfile = fopen(file, "a")) == NULL)
	{
		fprintf(stderr, "Failed to open/create log file: %s\n", file);
		exit(EXIT_FAILURE);
	}

	time(&curtime);
	fprintf(logfile, "Log begin: %s", ctime(&curtime));
	fprintf(logfile, "===================================\n");

	fflush(logfile);
}

void utilsLogCommit(int fatal, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(logfile, fmt, ap);
	va_end(ap);

	if (fatal)
	{
		fprintf(logfile, " ***FATAL ERROR***\n");
		exit(EXIT_FAILURE);
	}

	fprintf(logfile, "\n");
	fflush(logfile);
}

void utilsLogEnd(void)
{
	time_t curtime;
  	
	time(&curtime);
	fprintf(logfile, "=================================\n");
	fprintf(logfile, "Log end: %s\n", ctime(&curtime));

	fclose(logfile);
}