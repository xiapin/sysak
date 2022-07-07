#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
//#include <limits.h>
//#include <signal.h>
//#include <time.h>
#include <string.h>
#include <errno.h>
//#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include "schedmoni.h"

#define MAX_SYMS 300000
#define PERF_MAX_STACK_DEPTH	127

static int sym_cnt;

static int ksym_cmp(const void *p1, const void *p2)
{
	return ((struct ksym *)p1)->addr - ((struct ksym *)p2)->addr;
}

int load_kallsyms(struct ksym **pksyms)
{
	struct ksym *syms;
	FILE *f = fopen("/proc/kallsyms", "r");
	char func[256], buf[256];
	char symbol;
	void *addr;
	int i = 0;

	if (!f)
		return -ENOENT;

	syms = malloc(MAX_SYMS * sizeof(struct ksym));
	if (!syms) {
		fclose(f);
		return -ENOMEM;
	}

	while (!feof(f)) {
		if (!fgets(buf, sizeof(buf), f))
			break;
		if (sscanf(buf, "%p %c %s", &addr, &symbol, func) != 3)
			break;
		if (!addr)
			continue;
		syms[i].addr = (long) addr;
		syms[i].name = strdup(func);
		i++;
		if (i > MAX_SYMS) {
			printf("Warning: no space on ksym array!\n");
			break;
		}
	}
	fclose(f);
	sym_cnt = i;
	qsort(syms, sym_cnt, sizeof(struct ksym), ksym_cmp);
	*pksyms = syms;
	return 0;
}

struct ksym *ksym_search(long key, struct ksym *syms)
{
	int start = 0, end = sym_cnt;
	int result;

	/* kallsyms not loaded. return NULL */
	if (sym_cnt <= 0)
		return NULL;

	while (start < end) {
		size_t mid = start + (end - start) / 2;

		result = key - syms[mid].addr;
		if (result < 0)
			end = mid;
		else if (result > 0)
			start = mid + 1;
		else
			return &syms[mid];
	}

	if (start >= 1 && syms[start - 1].addr < key &&
	    key < syms[start].addr)
		/* valid ksym */
		return &syms[start - 1];

	/* out of range. return _stext */
	return &syms[0];
}

static int print_ksym(__u64 addr, struct ksym *psym, void *filep, int mod, int pos)
{
	int cnt = 0;
	struct ksym *sym;

	if (!addr)
		return 0;

	sym = ksym_search(addr, psym);
	if (mod == MOD_FILE)
		fprintf((FILE *)filep, "<0x%llx> %s\n", addr, sym->name);
	else if (mod == MOD_STRING)
		cnt = sprintf((char*)filep + pos, "<0x%llx> %s,", addr, sym->name);

	return cnt;
}

int print_stack(int fd, __u32 ret, int skip, struct ksym *syms, void *filep, int mod)
{
	int i, cnt = 0;
	__u64 ip[PERF_MAX_STACK_DEPTH] = {};

	if (bpf_map_lookup_elem(fd, &ret, &ip) == 0) {
		for (i = skip; i < PERF_MAX_STACK_DEPTH - 1; i++)
			cnt += print_ksym(ip[i], syms, filep, mod, cnt);
		if ((cnt > 0) && *((char*)(filep+cnt-1)) == ',')
			*((char*)(filep+cnt-1)) = ' ';
	} else {
		if ((int)(ret) < 0)
			fprintf(filep, "<0x0000000000000000>:error=%d\n", (int)(ret));
	}

	return cnt;
}

