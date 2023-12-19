#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include "kcore_utils.h"
#include "memcg_iter.h"

static struct btf *btf_handle = NULL;
int total_memcg_num = 0;
enum KERNEL_VERSION kernel_version = LINUX_4_19;

struct environment {
	int print_cg_num;                 /* unused */
} env = {
	.print_cg_num = 10000,
};

static int get_kernel_version()
{
    struct utsname kernel_info;
    char *release;
    long ver[16];
    int i = 0;

    if (uname(&kernel_info) < 0) {
        LOG_ERROR("uname error: %s\n", strerror(errno));
        return -1;
    }

    release = kernel_info.release;
    while (*release) {
        if (isdigit(*release)) {
            ver[i] = strtol(release, &release, 10);
            i++;
        } else {
            release++;
        }
    }

    if (ver[0] == 3) {
        kernel_version = LINUX_3_10;
    } else if (ver[0] == 4) {
        kernel_version = LINUX_4_19;
    } else {
        kernel_version = LINUX_5_10;
    }

    return 0;
}

static int caculate_offline(unsigned long start_memcg)
{   
    int offline_num = 0;
    unsigned long css, css_flags, cnt, iter = 0;
    long refcnt_value;
    unsigned int flags_value, offline_flag;
    char fileName[PATH_MAX];
    struct member_attribute *css_attr, *css_flag_attr, *refcnt_attr;
    struct member_attribute *cnt_attr, *ref_data_attr, *data_cnt_attr;

    css_attr = get_offset_no_cache("mem_cgroup", "css", btf_handle);
    if (!css_attr) {
        LOG_ERROR("get css offset of mem_cgroup failed!\n");
        return -1;
    }

    css_flag_attr = get_offset_no_cache("cgroup_subsys_state", 
                        "flags", btf_handle);
    if (!css_flag_attr) {
        LOG_ERROR("get flags offset of cgroup_subsys_state failed!\n");
        return -1;
    }

    refcnt_attr = get_offset_no_cache("cgroup_subsys_state", 
                    "refcnt", btf_handle);
    if (!refcnt_attr) {
        LOG_ERROR("get refcnt offset of cgroup_subsys_state failed!\n");
        return -1;
    }

    if (kernel_version == LINUX_5_10) {
        ref_data_attr = get_offset_no_cache("percpu_ref", "data", btf_handle);
        if (!ref_data_attr) {
            LOG_ERROR("get data offset of percpu_ref failed!\n");
            return -1;
        }
        data_cnt_attr = get_offset_no_cache("percpu_ref_data", "count", btf_handle);
        if (!data_cnt_attr) {
            LOG_ERROR("get cnt offset of percpu_ref_data failed!\n");
            return -1;
        }
    } else {
        cnt_attr = get_offset_no_cache("percpu_ref", "count", btf_handle);
        if (!cnt_attr) {
            LOG_ERROR("get cnt offset of percpu_ref failed!\n");
            return -1;
        }
    }

    for_each_mem_cgroup(iter, start_memcg, btf_handle) {
        css = iter + css_attr->offset;
        css_flags = css + css_flag_attr->offset;

        kcore_readmem(css_flags, &flags_value, sizeof(flags_value));
        if (kernel_version == LINUX_3_10) {
            offline_flag = !(flags_value & CSS_ONLINE);
        } else {
            offline_flag = flags_value & CSS_DYING;
        }
        
        if (offline_flag) {
            offline_num++;

            // in kernel 5.10, refcnt = css->refcnt->data->count
            // in other, refcnt = css->refcnt->count
            if (kernel_version == LINUX_5_10) {
                unsigned long ref_data, ref_data_val;
    
                ref_data = css + refcnt_attr->offset + ref_data_attr->offset;
                kcore_readmem(ref_data, &ref_data_val, sizeof(ref_data_val));

                cnt = ref_data_val + data_cnt_attr->offset;
                kcore_readmem(cnt, &refcnt_value, sizeof(refcnt_value));
            } else if (kernel_version == LINUX_4_19) {
                cnt = css + refcnt_attr->offset + cnt_attr->offset;
                kcore_readmem(cnt, &refcnt_value, sizeof(refcnt_value));
            } else {
                cnt = css + refcnt_attr->offset;
                kcore_readmem(cnt, &refcnt_value, sizeof(refcnt_value));
            }

            if (env.print_cg_num > 0) {
                memcg_get_name(iter, fileName, PATH_MAX, btf_handle);
                printf("cgroup path:%s\trefcount=%ld\n", fileName, refcnt_value);
                env.print_cg_num--;
            }
        }
        total_memcg_num++;
    }

    return offline_num;
}

static void show_usage(char *prog)
{
	const char *str =
	"   Usage: %s [OPTIONS]\n"
	"   Options:\n"
	"   -n PRINT_MAX_CG_NUM   Max offline memcg paths to printf(default 10000)\n"
    "   -h HELP               help\n"
    "   \n"

    "   EXAMPLE:\n "
    "   memcgoffline        # display number of offline memcg and all their paths.\n"
    "   memcgoffline -n 10  # display number of offline memcg and "
    "10 of offline memcg paths.\n"
	;

	fprintf(stderr, str, prog);
	exit(EXIT_FAILURE);
}

static int parse_args(int argc, char **argv, struct environment *env)
{
	int c, option_index;
    char *prog_name = "memcgoffline";

	for (;;) {
		c = getopt_long(argc, argv, "n:h", NULL, &option_index);
        if (c == -1)
            break;

		switch (c) {
			case 'n':
				env->print_cg_num = (int)strtol(optarg, NULL, 10);
                if (!errno)
                    return -errno;
				break;
			case 'h':
				show_usage(prog_name);	/* would exit */
				break;
			default:
				show_usage(prog_name);
		}
	}

    return 0;
}

struct btf *btf_init()
{
    char *btf_path;

    btf_path = prepare_btf_file();
    if (!btf_path)
        return NULL;
    
    return btf_load(btf_path);
}

void btf_uninit(struct btf *btf)
{
    return btf__free(btf);
}

int main(int argc, char *argp[])
{
	int offline_memcg = 0, ret = 0;

	ret = parse_args(argc, argp, &env);
    if (ret) {
        LOG_ERROR("parse arg error!\n");
        return -1;
    }

    ret = get_kernel_version();
    if (ret) {
        LOG_ERROR("get kernel version failed!");
        return -1;
    }

    btf_handle = btf_init();
    if (!btf_handle) {
        LOG_ERROR("btf init failed!\n");
        return -1;
    }

    ret = kcore_init();
    if (ret) {
        LOG_ERROR("kcore init failed!\n");
        goto uninit_btf;
    }

    ret = memcg_iter_init();
    if (ret) {
        LOG_ERROR("memcg_iter_init failed!\n");
        goto uninit_kcore;
    }

    offline_memcg = caculate_offline((unsigned long)NULL);
    if (offline_memcg < 0) {
        LOG_ERROR("caculate offline memcg failed!\n");
        ret = offline_memcg;
        goto uninit_kcore;
    }
    printf("Offline memory cgroup num: %d\n", offline_memcg);
    printf("Total memory cgroup num: %d\n", total_memcg_num);

uninit_kcore:
    kcore_uninit();
uninit_btf:
    btf_uninit(btf_handle);

    return ret;
}
