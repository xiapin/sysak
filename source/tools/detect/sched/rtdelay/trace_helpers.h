#ifndef __TRACE_HELPERS_H
#define __TRACE_HELPERS_H

#include <stdbool.h>

#define NSEC_PER_SEC		1000000000ULL

struct ksym {
	const char *name;
	unsigned long addr;
};

struct ksyms;

struct ksyms *ksyms__load(void);
void ksyms__free(struct ksyms *ksyms);
const struct ksym *ksyms__map_addr(const struct ksyms *ksyms,
				   unsigned long addr);
const struct ksym *ksyms__get_symbol(const struct ksyms *ksyms,
				     const char *name);

unsigned long long get_ktime_ns(void);

#endif /* __TRACE_HELPERS_H */
