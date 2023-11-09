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
#include <linux/version.h>


extern unsigned long page_shift;
#define MAX_ORDER		11
#define PAGE_SIZE		(1UL << page_shift)
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
extern unsigned long memstart_addr;
#define PAGE_STRUCT_SIZE    64

#ifdef __aarch64__ /*arm arch*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0) /*kernel 510*/
#define VA_BITS (48)
#define SZ_2M               0x00200000
#define STRUCT_PAGE_MAX_SHIFT (6)
#define VA_BITS_MIN (48)

#define _PAGE_END(va)       (-((unsigned long )(1) << ((va) - 1)))
#define _PAGE_OFFSET(va)    (-((unsigned long )(1) << (va)))
#define PAGE_OFFSET     (_PAGE_OFFSET(VA_BITS))
#define VMEMMAP_SIZE ((_PAGE_END(VA_BITS_MIN) - PAGE_OFFSET) \
        >> (page_shift - STRUCT_PAGE_MAX_SHIFT))
#define PHYS_OFFSET     (memstart_addr)
#define VMEMMAP_START       (-VMEMMAP_SIZE - SZ_2M)
#define vmemmap         (VMEMMAP_START - (memstart_addr >> page_shift)*PAGE_STRUCT_SIZE)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 9)/*kernel 419*/
#define VA_BITS (48)
#define VA_START        ((unsigned long )(0xffffffffffffffff) - \
    ((unsigned long )(1) << VA_BITS) + 1)
#define PAGE_OFFSET     ((unsigned long )(0xffffffffffffffff) - \
    ((unsigned long )(1) << (VA_BITS - 1)) + 1)
#define STRUCT_PAGE_MAX_SHIFT (6)
#define VMEMMAP_SIZE ((unsigned long )(1) << (VA_BITS - page_shift - 1 + STRUCT_PAGE_MAX_SHIFT))
#define VMEMMAP_START       (PAGE_OFFSET - VMEMMAP_SIZE)
#define vmemmap         (VMEMMAP_START - (memstart_addr >> page_shift)*PAGE_STRUCT_SIZE)

#else /*others*/
#define SZ_64K              0x00010000
#define PAGE_OFFSET     (unsigned long )(0xffffffc000000000)
#define VMALLOC_END     (PAGE_OFFSET - (unsigned long)(0x400000000) - SZ_64K)
#define vmemmap         ((struct page *)(VMALLOC_END + SZ_64K))

#endif /*end to check ver, arm arch*/
#define PFN_TO_VIRT(pfn)    (((unsigned long)((pfn) - PHYS_OFFSET) | PAGE_OFFSET) + ((pfn) << page_shift))
#define PFN_TO_PAGE(pfn)    (vmemmap + (pfn) * PAGE_STRUCT_SIZE)
#define is_kvaddr(kvaddr) (!!(kvaddr >= PAGE_OFFSET))
#else /*x86 arch*/

#define PFN_TO_VIRT(pfn)    (page_offset_base + ((pfn) << page_shift))
#define PFN_TO_PAGE(pfn)    (vmemmap_base + (pfn) * PAGE_STRUCT_SIZE)
#define is_kvaddr(kvaddr) (!!(kvaddr >= page_offset_base))
#endif
#define max_pfn (g_max_phy_addr>>12)

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
