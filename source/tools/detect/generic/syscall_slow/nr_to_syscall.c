#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#define MAX_NR	2048

char path[] = "/usr/include/asm/unistd_64.h";

static inline bool isdigtal(char c)
{
	if(c <= '9' && c >= '0')
		return true;
	else
		return false;
}

static int parse(char *p, char *arry[])
{
	int nr;
	char *t, *new;

	nr = -1;
	t = p;
	while(*t != '\0') {
		if ((*t == ' ') && (p != t)) {
			size_t size = t - p + 1;
			new = malloc(size);
			if (!new) {
				printf(" malloc %ld fail\n", size);
				break;
			}
			memset(new, 0, size);
			strncpy(new, p, size - 1);
			t++;
			while (!isdigtal(*t) && (*t != '\0')) {
				t++;
			}
			if (*t!='\0')
				nr = strtol(t, NULL, 10);

			if (nr > MAX_NR)
				break;
			arry[nr] = new;
			break;
		}
		t++;
	}

	return nr;
}

int nr_to_syscall(int argc, char *arry[])
{
	int idx = 0;
	FILE *fp;
	char *p, buff[128];

	fp = fopen(path, "r");
	if (!fp)
		return errno;

	while (fgets(buff, sizeof(buff), fp)) {
		p = strstr(buff, "__NR_");
		if (p) {
			p = p+5;	/* skip __NR_ */
			parse(p, arry);
			idx++;
		}
		memset(buff, '\0', sizeof(buff));
	}

	if (idx > MAX_NR) {
		printf("There are %d (>2048) syscalls, overflow\n!", idx);
	}
	fclose(fp);
	return 0;
}
