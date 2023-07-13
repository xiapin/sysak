#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>

#include "kcore_utils.h"

#define LEN             (128)

static struct proc_kcore_data proc_kcore_data = { 0 };
static struct proc_kcore_data *pkd = &proc_kcore_data;

static int kcore_fd = 0;

/*
 * Routines of kcore, i.e., /proc/kcore
 */
uintptr_t lookup_kernel_symbol(const char *symbol_name)
{
	const char *kallsyms_file = "/proc/kallsyms";
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

static int kcore_elf_init()
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

int kcore_init() 
{
    if ((kcore_fd = open("/proc/kcore", O_RDONLY)) < 0) {
		perror("open: /proc/kcore");
		return -1;
	}

    if (kcore_elf_init())
		goto failed;

    return 0;
    
failed:
	close(kcore_fd);
	return -1;
}

void kcore_uninit(void)
{
	if (pkd->elf_header)
		free(pkd->elf_header);
	if (kcore_fd > 0)
		close(kcore_fd);
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

static void stripWhiteSpace(char *str)
{   
    char tmp_str[strlen(str)];
    int i, j = 0;

    for (i = 0; str[i] != '\0'; i++) {
        if (str[i] != ' ' && str[i] != '\t' 
                && str[i] != '\n') {
            tmp_str[j] = str[i];
            j++;
        }
    }

    tmp_str[j] = '\0';
    strcpy(str, tmp_str);

    return;
}

static int do_cmd(const char *cmd, char *result, int len)
{
    FILE *res;
    char region[LEN] = {0};
    char *str;

    res = popen(cmd, "r");
    if (res == NULL) {
        printf("get region id failed\n");
        return -1;
    }

    if (feof(res)) {
        printf("cmd line end\n");
        return 0;
    }
    fgets(region, sizeof(region)-1, res);
    str = region;
    stripWhiteSpace(str);
    /* skip \n */
    strncpy(result, str, len - 1);
    result[len - 1] = '\0';
    pclose(res);
    return 0;
}

static int download_btf()
{
    char region[LEN] = {0};
    char arch[LEN] = {0};
    char kernel[LEN] = {0};
    char dw[LEN+LEN] = {0};
    char timeout[LEN] = "-internal";
    char sysak_path[LEN] = "/boot";
    char *curl_cmd = "curl -s --connect-timeout 2 http://100.100.100.200/latest/meta-data/region-id 2>&1";
    char *arch_cmd = "uname -m";
    char *kernel_cmd = "uname -r";
    char *tmp;

    do_cmd(curl_cmd, region, LEN);
    if (!strstr(region,"cn-")) {
        strcpy(region, "cn-hangzhou");
        memset(timeout, 0, sizeof(timeout));
    }

    do_cmd(arch_cmd, arch, LEN);

    do_cmd(kernel_cmd, kernel, LEN);

    if((tmp = getenv("SYSAK_WORK_PATH")) != NULL)
    {
        memset(sysak_path, 0, sizeof(sysak_path));
        strcpy(sysak_path, tmp);
        strcat(sysak_path, "/tools/");
        strcat(sysak_path, kernel);
    }

    snprintf(dw, LEN + LEN + LEN, "wget -T 5 -t 2 -q -O %s/vmlinux-%s https://sysom-cn-%s.oss-cn-%s%s.aliyuncs.com/home/hive/btf/%s/vmlinux-%s", sysak_path, kernel, &region[3],&region[3], timeout,arch, kernel);

    do_cmd(dw, kernel, LEN);
    return 0;
}

static int check_btf_file(char *btf)
{
    struct stat fstat;
    int ret = 0;

    ret = stat(btf, &fstat);
    if (ret)
        return -1;
    if (fstat.st_size < 10*1024)
        return -1;

    return 0;
}

char *prepare_btf_file()
{
    static char btf[LEN] = {0};
    char ver[LEN] = {0};
    char *cmd = "uname -r";

    do_cmd(cmd, ver, LEN);

    if (getenv("SYSAK_WORK_PATH") != NULL)
        sprintf(btf,"%s/tools/%s/vmlinux-%s", getenv("SYSAK_WORK_PATH"), ver, ver);
    else
        sprintf(btf,"/boot/vmlinux-%s", ver);

    if (check_btf_file(btf)) {
        download_btf();
    };

    if (check_btf_file(btf)) {
        LOG_ERROR("btf file:%s not found \n", btf);
        return NULL;
    }

    return btf;
}