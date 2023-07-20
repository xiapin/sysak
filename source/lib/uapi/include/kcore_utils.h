#ifndef __KCORE_UTILS_H
#define __KCORE_UTLIS_H 

#include <inttypes.h>
#include <sys/types.h>
#include <elf.h>

#define BUFF_MAX		4096
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

/* struct to record the kcore elf file data*/
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


/**
 * lookup_kernel_symbol - look up kernel symbol address from /proc/kallsyms
 * 
 * @symbol_name: kernel symbol name to look up.
 * @return: the address of the kernel symbol. 
 * 
 */
uintptr_t lookup_kernel_symbol(const char *symbol_name);

/* prepare_btf_file - check exist btf file, if not exist, download it */
char *prepare_btf_file();

/* open /proc/kcore and read necessary data to interpret kcore */
int kcore_init();

/* close /proc/kcore and do some cleanup */
void kcore_uninit();

/**
 * kcore_readmem - read data of certain kernel address from kcore
 * 
 * @kvaddr: kernel address to read.
 * @buf: buf for readed data.
 * @size: size of the data to read. 
 * @return: size of the data beeing read if success.
 * 
 * Note: must call after kcore_init()
 */
ssize_t kcore_readmem(unsigned long kvaddr, void *buf, ssize_t size);


#endif