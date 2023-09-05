/*
 * pod memory tool
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#include <ctype.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

#include "cache.h"
#define MAX_CGROUP 256

extern int scan_pageflags(struct options * opt);
extern int scan_pageflags_nooutput(struct options * opt,char *res);
extern int memcg_cgroup_file(const char *cgroupfile);
extern int memcg_cgroup_path(const char *cgrouppath);
extern int offset_init(void);
extern int sym_uninit(void);

extern "C" {
char* scanall();
int monitor_init(const char* proc);
void monitor_exit();
}

struct options opt = {0};

struct proc_kcore_data {
	unsigned int flags;
	unsigned int segments;
	char *elf_header;
	size_t header_size;
	Elf64_Phdr *load64;
	Elf64_Phdr *notes64;
	Elf32_Phdr *load32;
	Elf32_Phdr *notes32;
	void *vmcoreinfo;
	unsigned int size_vmcoreinfo;
};

static struct proc_kcore_data proc_kcore_data = { 0 };
static struct proc_kcore_data *pkd = &proc_kcore_data;

static int kcore_fd = 0;
static int kpageflags_fd = 0;
static int kpagecgroup_fd = 0;
uint64_t g_max_phy_addr;


/*
 * kernel	vmemmap_base		page_offset_base
 * 3.10		0xffffea0000000000UL	0xffff880000000000UL
 * 4.9		0xffffea0000000000UL	0xffff880000000000UL
 * 4.19		0xffffea0000000000UL	0xffff888000000000UL
 *
 * We use default vmemmap_base and page_offset_base values on kernel 4.9,
 * which is the same on kernel 3.10, and reassign these two values on
 * kernel 4.19 due to kaslr, by kcore.
 */
unsigned long vmemmap_base = 0xffffea0000000000UL;
unsigned long page_offset_base = 0xffff880000000000UL;
unsigned long memstart_addr = 0x0;
unsigned long page_shift = 0;

/*
 * Routines of kpageflags, i.e., /proc/kpageflags
 */
ssize_t kpageflags_read(void *buf, size_t count, off_t offset)
{
	return pread(kpageflags_fd, buf, count, offset);
}

ssize_t kpagecgroup_read(void *buf, size_t count, off_t offset)
{
    ssize_t ret = 0;
    if (kpagecgroup_fd > 0)
	    ret = pread(kpagecgroup_fd, buf, count, offset);

    return ret;
}

/*
 * Routines of kcore, i.e., /proc/kcore
 */
uintptr_t lookup_kernel_symbol(const char *symbol_name, const char *proc)
{
        std::string tmp(proc);
        tmp +=  "/proc/kallsyms";
	const char *kallsyms_file = tmp.c_str();
	FILE *fp;
	char line[BUFF_MAX];
	char *pos;
	uintptr_t addr = -1UL;

	fp = fopen(kallsyms_file, "r");
	if (fp == NULL) {
		perror("fopen: /proc/kallsyms");
		return -1;
	}

	while (fgets(line, BUFF_MAX, fp)) {
		if ((pos = strstr(line, symbol_name)) == NULL)
			continue;

		/* Remove trailing newline */
		line[strcspn(line, "\n")] = '\0';

		/* Exact match */
		if (pos == line || !isspace(*(pos - 1)))
			continue;
		if (!strcmp(pos, symbol_name)) {
			addr = strtoul(line, NULL, 16);
			break;
		}
	}

	if (addr == -1UL)
		LOG_ERROR("failed to lookup symbol: %s\n", symbol_name);

	fclose(fp);
	return addr;
}

static int kcore_elf_init(void)
{
	Elf64_Ehdr *elf64;
	Elf64_Phdr *load64;
	Elf64_Phdr *notes64;
	char eheader[MAX_KCORE_ELF_HEADER_SIZE];
	size_t load_size, notes_size;

	if (read(kcore_fd, eheader, MAX_KCORE_ELF_HEADER_SIZE) !=
			MAX_KCORE_ELF_HEADER_SIZE) {
		perror("read: /proc/kcore ELF header");
		return -1;
	}

	elf64 = (Elf64_Ehdr *)&eheader[0];
	notes64 = (Elf64_Phdr *)&eheader[sizeof(Elf64_Ehdr)];
	load64 = (Elf64_Phdr *)&eheader[sizeof(Elf64_Ehdr) +
					sizeof(Elf64_Phdr)];

	pkd->segments = elf64->e_phnum - 1;

	notes_size = load_size = 0;
	if (notes64->p_type == PT_NOTE)
		notes_size = notes64->p_offset + notes64->p_filesz;
	if (notes64->p_type == PT_LOAD)
		load_size = (unsigned long)(load64+(elf64->e_phnum)) -
				(unsigned long)elf64;

	pkd->header_size = MAX(notes_size, load_size);
	if (!pkd->header_size)
		pkd->header_size = MAX_KCORE_ELF_HEADER_SIZE;

	if ((pkd->elf_header = (char *)malloc(pkd->header_size)) == NULL) {
		perror("malloc: /proc/kcore ELF header");
		return -1;
	}

	memcpy(&pkd->elf_header[0], &eheader[0], pkd->header_size);
	pkd->notes64 = (Elf64_Phdr *)&pkd->elf_header[sizeof(Elf64_Ehdr)];
	pkd->load64 = (Elf64_Phdr *)&pkd->elf_header[sizeof(Elf64_Ehdr) +
						     sizeof(Elf64_Phdr)];

	return 0;
}

static int kcore_init(const char* proc)
{
    unsigned long vmemmap_symbol_addr;
    unsigned long page_offset_symbol_addr;
    unsigned long memstart_addr_addr;
    std::string tmp(proc);
    tmp += "/proc/kcore"; 
	int size;

	if ((kcore_fd = open(tmp.c_str(), O_RDONLY)) < 0) {
		perror("open: /proc/kcore");
		return -1;
	}

	if (kcore_elf_init())
		goto failed;

	vmemmap_symbol_addr = lookup_kernel_symbol("vmemmap_base",proc);
	if (vmemmap_symbol_addr == (unsigned long )-1) {
		LOG_WARN("continue to use default vmemmap_base: 0x%lx\n",
				vmemmap_base);
	} else {
		size = kcore_readmem(vmemmap_symbol_addr, &vmemmap_base, 8);
		if (size < 8)
			goto failed;
	}

	page_offset_symbol_addr = lookup_kernel_symbol("page_offset_base", proc);
	if (page_offset_symbol_addr == (unsigned long)-1) {
		LOG_WARN("continue to use default page_offset_base: 0x%lx\n",
				page_offset_base);
	} else {
		size = kcore_readmem(page_offset_symbol_addr, &page_offset_base, 8);
		if (size < 8)
			goto failed;
	}

	memstart_addr_addr = lookup_kernel_symbol("memstart_addr",proc);
	if (memstart_addr_addr == (unsigned long)-1) {
		LOG_WARN("continue to use default memstart_addr_base: 0x%lx\n",
				memstart_addr);
	} else {
		size = kcore_readmem(memstart_addr_addr, &memstart_addr, 8);
		if (size < 8)
			goto failed;
	}
	return 0;

failed:
	close(kcore_fd);
	return -1;
}

/*
 * We may accidentally access invalid pfns on some kernels
 * like 4.9, due to known bugs. Just skip it.
 */
ssize_t kcore_readmem(unsigned long kvaddr, void *buf, ssize_t size)
{
	Elf64_Phdr *lp64;
	unsigned long offset = -1UL;
	ssize_t read_size;
	unsigned int i;

	for (i = 0; i < pkd->segments; i++) {
		lp64 = pkd->load64 + i;
		if ((kvaddr >= lp64->p_vaddr) &&
			(kvaddr < (lp64->p_vaddr + lp64->p_memsz))) {
			offset = (off_t)(kvaddr - lp64->p_vaddr) +
					(off_t)lp64->p_offset;
			break;
		}
	}
	if (i == pkd->segments) {
		for (i = 0; i < pkd->segments; i++) {
			lp64 = pkd->load64 + i;
			LOG_DEBUG("%2d: [0x%lx, 0x%lx)\n", i, lp64->p_vaddr,
					lp64->p_vaddr + lp64->p_memsz);
		}
		//printf("invalid kvaddr 0x%lx\n", kvaddr);
		goto failed;
	}

	if (lseek(kcore_fd, offset, SEEK_SET) < 0) {
		perror("lseek: /proc/kcore");
		goto failed;
	}

	read_size = read(kcore_fd, buf, size);
	if (read_size < size) {
		perror("read: /proc/kcore");
		goto failed;
	}

	return read_size;

failed:
	return -1;
}

static void kcore_exit(void)
{
	if (pkd->elf_header)
		free(pkd->elf_header);
	if (kcore_fd > 0)
		close(kcore_fd);
}


static uint64_t get_max_phy_addr(const char *proc)
{
        std::string iofile(proc);
        iofile += "/proc/iomem"; 
	const char *iomem_file = iofile.c_str();
        printf("%s\n",iomem_file);
	FILE *fp = NULL;
	char line[BUFF_MAX], *pos, *end = NULL;
	uint64_t max_phy_addr = 0;

	fp = fopen(iomem_file, "r");
	if (fp == NULL) {
		perror("fopen: /proc/iomem");
		return 0ULL;
	}

	while (fgets(line, BUFF_MAX, fp)) {
		if (strstr(line, "System RAM") == NULL)
			continue;

		pos = strchr(line, '-');
		if (pos == NULL)
			break;
		pos++;

		max_phy_addr = strtoull(pos, &end, 16);
		if (end == NULL) {
			perror("strtoull: max_phy_addr");
			max_phy_addr = 0;
			break;
		}
	}

	fclose(fp);
	return max_phy_addr;
}

static int setup(const char* proc)
{
        std::string filename(proc);
        std::string tmp;
	g_max_phy_addr = get_max_phy_addr(proc);
	if (g_max_phy_addr == 0ULL) {
		g_max_phy_addr = 64 * 1024 * 1024 * 1024;
		LOG_ERROR("failed to get max physical address\n");
	}
	LOG_DEBUG("max physical address = %#lx\n", g_max_phy_addr);

        tmp = filename+"/proc/kpageflags";
	kpageflags_fd = open(tmp.c_str(), O_RDONLY);
	if (kpageflags_fd < 0) {
		perror("open: /proc/kpageflags");
		return -1;
	}
        tmp = filename+"/proc/kpagecgroup";
    kpagecgroup_fd = open(tmp.c_str(), O_RDONLY);
	if (kpagecgroup_fd < 0) {
		perror("open: /proc/kpagecgroup");
	}
	if (kcore_init(proc) < 0) {
		LOG_ERROR("failed to init kcore\n");
		return -1;
	}

    return offset_init();
}

static void cleanup(void)
{
	if (kpageflags_fd > 0)
		close(kpageflags_fd);

    if (kpagecgroup_fd > 0) {
        close(kpagecgroup_fd);
    }
    sym_uninit();
    kcore_exit();
}

static int get_pageshift()
{
    int page_size = getpagesize();

    if(page_size <= 0 )
    {
        LOG_ERROR("failed to get page size\n");
        return -1;
    }
    while (page_size > 1) {
        page_size >>= 1;
        page_shift++;
    }
    return 0;
}

static void show_usage(void)
{
	LOG_INFO("Usage: %s [OPTIONS]\n"
		 "\n"
		 "  -h, --help           display this help and exit\n"
		 "\n"
		 "Supported options:\n"
		 "  -m, pod cache tools \n"
		 "\n"
		 );
}

int monitor_init(const char* proc)
{
    int ret = get_pageshift();
    if (ret != 0) {
        printf("failed to page shift\n");
        return ret;
    }
    ret = setup(proc);
    if (ret != 0) {
        printf("failed to setup\n");
        return ret;
    }
   return 0;
}

void monitor_exit()
{
    cleanup();
}

char* scanall()
{
    const char *filename="/tmp/.memcg.txt";
    char *outputres = NULL;
    
    opt.rate = 100;
    opt.top = 5;
    int count = memcg_cgroup_file(filename);
    if (count < 0)
          return NULL;
    count = (count < 0 || count>MAX_CGROUP) ? MAX_CGROUP:count;
    count = 350 * opt.top * count;
    outputres = (char*)malloc(count);
    memset(outputres,0,count);

    scan_pageflags_nooutput(&opt,outputres);
    return outputres;
}
