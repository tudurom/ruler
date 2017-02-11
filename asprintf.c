#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "asprintf.h"

int
asprintf(char **str, const char *format, ...)
{
	int size = 0;
	va_list args;

	va_start(args, format);
	size = vsnprintf(NULL, 0, format, args);

	if (size < 0)
		size = -1;
	*str = malloc((size + 1) * sizeof(char));
	if (str == NULL)
		return -1;

	size = vsprintf(*str, format, args);
	va_end(args);

	return size;
}
