#include <assert.h>
#include <rules.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

char *strdup(const char *s)
{
	if (!ENV_RAMSTAGE)
		dead_code();	/* This can't be used without malloc(). */

	size_t sz = strlen(s) + 1;
	char *d = malloc(sz);
	if (d)
		memcpy(d, s, sz);
	return d;
}

char *strconcat(const char *s1, const char *s2)
{
	if (!ENV_RAMSTAGE)
		dead_code();	/* This can't be used without malloc(). */

	size_t sz_1 = strlen(s1);
	size_t sz_2 = strlen(s2);
	char *d = malloc(sz_1 + sz_2 + 1);
	if (d) {
		memcpy(d, s1, sz_1);
		memcpy(d + sz_1, s2, sz_2 + 1);
	}
	return d;
}

size_t strnlen(const char *src, size_t max)
{
	size_t i = 0;
	while ((*src++) && (i < max))
		i++;
	return i;
}

size_t strlen(const char *src)
{
	size_t i = 0;
	while (*src++)
		i++;
	return i;
}

char *strchr(const char *s, int c)
{
	do {
		if (*s == c)
			return (char *)s;
	} while (*s++);

	return NULL;
}

char *strrchr(const char *s, int c)
{
	char *p = NULL;

	do {
		if (*s == c)
			p = (char *)s;
	} while (*s++);

	return p;
}

char *strncpy(char *to, const char *from, int count)
{
	char *ret = to;
	char data;

	while (count > 0) {
		count--;
		data = *from++;
		*to++  = data;
		if (data == '\0')
			break;
	}

	while (count > 0) {
		count--;
		*to++ = '\0';
	}
	return ret;
}

char *strcpy(char *dst, const char *src)
{
	char *ptr = dst;

	while (*src)
		*dst++ = *src++;
	*dst = '\0';

	return ptr;
}

int strcmp(const char *s1, const char *s2)
{
	int r;

	while ((r = (*s1 - *s2)) == 0 && *s1) {
		s1++;
		s2++;
	}
	return r;
}

int strncmp(const char *s1, const char *s2, int maxlen)
{
	int i;

	for (i = 0; i < maxlen; i++) {
		if ((s1[i] != s2[i]) || (s1[i] == '\0'))
			return s1[i] - s2[i];
	}

	return 0;
}

unsigned int skip_atoi(char **s)
{
	unsigned int i = 0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}
