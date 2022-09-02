/*
 * Page scan tool
 */
#ifndef _PAGESCAN_UTIL_H
#define _PAGESCAN_UTIL_H

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_ORDER		11
#define PAGE_SHIT		12
#define PAGE_SIZE		(1UL << PAGE_SHIT)
#define HUGE_SIZE		(PAGE_SIZE * HUGE_PAGE_NR)
#define BUFF_MAX		4096
#define SIZE_KB			(1UL << 10)
#define SIZE_MB			(1UL << 20)
#define MAX_KCORE_ELF_HEADER_SIZE   32768


#ifdef DEBUG
#define LOG_DEBUG(...)	fprintf(stderr, __VA_ARGS__)
#else
#define LOG_DEBUG(...)	do { } while (0)
#endif /* DEBUG */

#define LOG_INFO(...)	fprintf(stdout, __VA_ARGS__)
#define LOG_WARN(...)	fprintf(stderr, __VA_ARGS__)
#define LOG_ERROR(...)	fprintf(stderr, __VA_ARGS__)

#define MIN(a,b)       (((a)<(b))?(a):(b))
#define MAX(a,b)       (((a)>(b))?(a):(b))

extern unsigned long vmemmap_base;
extern unsigned long page_offset_base;
extern uint64_t g_max_phy_addr;
#define PAGE_STRUCT_SIZE    64

#define PFN_TO_VIRT(pfn)    (page_offset_base + ((pfn) << PAGE_SHIT))
#define PFN_TO_PAGE(pfn)    (vmemmap_base + (pfn) * PAGE_STRUCT_SIZE)
#define max_pfn (g_max_phy_addr>>12)
#define is_kvaddr(kvaddr) (!!(kvaddr >= page_offset_base))

struct options {
    bool podmem;
    bool fullscan;
    char *cgroupfile;
    char *cgroup;
    unsigned int rate;
    unsigned int top;
};

#define KPF_SIZE	8
ssize_t kpageflags_read(void *buf, size_t count, off_t offset);
ssize_t kpagecgroup_read(void *buf, size_t count, off_t offset);
uintptr_t lookup_kernel_symbol(const char *symbol_name);
ssize_t kcore_readmem(unsigned long kvaddr, void *buf, ssize_t size);
#endif /* _PAGESCAN_UTIL_H */
