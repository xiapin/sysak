#ifndef __MEMCG_ITER_H_
#define __MEMCG_ITER_H_

#include "btfparse.h"

#define PATH_MAX        (2048)
#define LEN             (255)
#define CSS_DYING       (1 << 4)     /* css is dying */

/* iterator function of "for_each_mem_cgroup" */
unsigned long _mem_cgroup_iter(unsigned long root, unsigned long prev,
                struct btf* handle);

/* find out and set root_mem_cgroup from kallsyms*/
int memcg_iter_init();

/* Iter all memory cgroups, must call after memcg_iter_init() */
#define for_each_mem_cgroup(iter, start, btf)           \
    for (iter = _mem_cgroup_iter(start, (unsigned long)NULL, btf);  \
         iter != (unsigned long)NULL;              \
         iter = _mem_cgroup_iter(start, iter, btf))

/* 
 * get member offset of certain struct, need to read from btf file,
 * (don't call it in loop which may cause huge overhead)
 */
struct member_attribute *get_offset_no_cache(char *struct_name, 
                            char *member_name, struct btf *handle);

int get_member_offset(char *struct_name, char *member_name, 
        struct btf *handle);

void memcg_get_name(unsigned long memcg, char *name,
                int len, struct btf *btf_handle);

#endif