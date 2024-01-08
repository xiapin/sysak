#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "memcg_iter.h"
#include "kcore_utils.h"

static unsigned long root_mem_cgroup;
extern enum KERNEL_VERSION kernel_version;

struct member_attribute *get_offset_no_cache(char *struct_name, 
                            char *member_name, struct btf *handle)
{
    struct member_attribute *att;

    att = btf_find_struct_member(handle, struct_name, member_name);
    if (!att) {
        return NULL;
    }

    att->offset = att->offset/8;
         
    return att;
}

int get_member_offset(char *struct_name, char *member_name, struct btf *handle)
{
    char prefix[LEN] = "struct ";
    
    strcat(prefix, struct_name);

    return btf_get_member_offset(handle, prefix, member_name)/8;
}

static unsigned long _cg_next_child(unsigned long pos, unsigned long parent,
                        struct btf *btf_handle)
{
    struct member_attribute *att, *att2;
    unsigned long next;

    att = get_offset_no_cache("cgroup", "sibling", btf_handle);
    if (!att)
        return 0;

    att2 = get_offset_no_cache("cgroup", "children", btf_handle);
    if (!att2)
        return 0;

    if(!pos) {
        kcore_readmem(parent + att2->offset, &next, sizeof(next));
        next = next - att->offset;
    } else {
        kcore_readmem(pos + att->offset, &next, sizeof(next));
        next = next - att->offset;
    }

    if(next + att->offset != parent + att2->offset)
        return next;

    return 0;
}

static unsigned long _css_next_child(unsigned long pos, unsigned long parent,
                        struct btf *btf_handle)
{
    struct member_attribute *att, *att2;
    unsigned long next;

    att = get_offset_no_cache("cgroup_subsys_state", "sibling", btf_handle);
    if (!att)
        return 0;

    att2 = get_offset_no_cache("cgroup_subsys_state", "children", btf_handle);
    if (!att2)
        return 0;

    if(!pos) {
        kcore_readmem(parent + att2->offset, &next, sizeof(next));
        next = next - att->offset;
    } else {
        kcore_readmem(pos + att->offset, &next, sizeof(next));
        next = next - att->offset;
    }

    if(next + att->offset != parent + att2->offset)
        return next;

    return 0;
}

static unsigned long cg_to_memcg(unsigned long cgroup, struct btf *btf_handle)
{
    struct member_attribute *cg_subsys_att, *memcg_css_att;
    unsigned long css_offset, css;
    // normally, mem_cgroup_subsys_id = 3 (without cgroup debug subsys)
    const int mem_cgroup_subsys_id = 3;

    cg_subsys_att = get_offset_no_cache("cgroup", "subsys", btf_handle);
    if (!cg_subsys_att)
        return 0;

    css_offset = cgroup + cg_subsys_att->offset + (mem_cgroup_subsys_id * sizeof(unsigned long));
    kcore_readmem(css_offset, &css, sizeof(css));

    memcg_css_att = get_offset_no_cache("mem_cgroup", "css", btf_handle);
    if (!memcg_css_att)
        return 0;

    // equal to mem_cgroup_from_css()
    return css - memcg_css_att->offset;
}

unsigned long _mem_cgroup_iter(unsigned long root, unsigned long prev,
                struct btf *btf_handle)
{
    struct member_attribute *att, *att2;
    unsigned long css, root_css;
    unsigned long memcg = 0;
    unsigned long pos = 0;
    unsigned long next = 0;
    unsigned long tmp1, tmp2;

    if(!root)
        root = root_mem_cgroup; 
    if(!prev)
        return root;
    
    att = get_offset_no_cache("mem_cgroup", "css", btf_handle);
    if (!att)
        return 0;

    pos = prev;
    //kcore_readmem(pos + att->offset, &css, sizeof(css));
    css = pos + att->offset;
    //kcore_readmem(root+att->offset, &root_css, sizeof(root_css));
    root_css = root + att->offset;

    if (kernel_version == LINUX_3_10) {
        struct member_attribute *css_cg_att, *cg_parent_att;
        unsigned long cg, root_cg;
        unsigned long cg_tmp1, cg_tmp2;

        css_cg_att = get_offset_no_cache("cgroup_subsys_state", "cgroup", btf_handle);
        if (!css_cg_att)
            return 0;

        cg_parent_att = get_offset_no_cache("cgroup", "parent", btf_handle);
        if (!cg_parent_att)
            return 0;

        kcore_readmem(css + css_cg_att->offset, &cg, sizeof(cg));
        kcore_readmem(root_css + css_cg_att->offset, &root_cg, sizeof(root_cg));

        next = _cg_next_child(0, cg, btf_handle);
        if (!next) {
            cg_tmp1 = cg;
            while (cg_tmp1 != root_cg) {
                kcore_readmem(cg_tmp1 + cg_parent_att->offset, &cg_tmp2, sizeof(cg_tmp2));
                next = _cg_next_child(cg_tmp1, cg_tmp2, btf_handle);
                if (next)
                    break;
                cg_tmp1 = cg_tmp2;
            }
        }
    } else {
        att2 = get_offset_no_cache("cgroup_subsys_state", "parent", btf_handle);
        if (!att2)
            return 0;

        next = _css_next_child(0, css, btf_handle);
        if(!next)
        {
            tmp1 = css;
            while(tmp1 != root_css)
            {
                kcore_readmem(tmp1 + att2->offset, &tmp2, sizeof(tmp2));
                next = _css_next_child(tmp1, tmp2, btf_handle);
                if(next)
                    break;
                tmp1 = tmp2;
            }
        }
    }

    if(!next)
        return 0;

    if (kernel_version == LINUX_3_10) {
        memcg = cg_to_memcg(next, btf_handle);
    } else {
        memcg = next - att->offset;
    }

    return memcg;
}

int memcg_iter_init()
{
    unsigned long tmp;
    size_t size;

    tmp = lookup_kernel_symbol("root_mem_cgroup");
    if (tmp == (unsigned long )-1) {
        LOG_ERROR("unable to get root_mem_cgroup\n");
        return -1;
    } else {
        size = kcore_readmem(tmp, &root_mem_cgroup, 8);
        if (size < 8) {
            LOG_ERROR("get incorrect address where root_mem_cgroup point to\n");
            return -1;
        }
    }

    return 0;
}

static int prepend(char **buffer, int *buflen, const char *str, 
            int namelen, int off)
{
    *buflen -= namelen + off;
    if (*buflen < 0)
        return -1; 
    *buffer -= namelen + off;
    if (off)
        **buffer = '/';
    memcpy(*buffer + off, str, namelen);
    return 0;
}

static int cgroup_path(unsigned long cgrp, char *buf, 
            int buflen, struct btf *btf_handle)
{
    int ret  = -1;
    char *start;
    unsigned long cgp;
    char tmpname[PATH_MAX];
    struct member_attribute *cg_pa_att, *cg_name_att;
    struct member_attribute *cgn_name_attr;

    cg_pa_att = get_offset_no_cache("cgroup", "parent", btf_handle);
    if (!cg_pa_att)
        return -1;
    
    cg_name_att = get_offset_no_cache("cgroup", "name", btf_handle);
    if (!cg_name_att)
        return -1;

    cgn_name_attr = get_offset_no_cache("cgroup_name", "name", btf_handle);
    if (!cgn_name_attr)
        return -1;
    

    kcore_readmem(cgrp + cg_pa_att->offset, &cgp, sizeof(cgp));
    if (!cgp) {
        if (strncpy(buf, "/", buflen) == NULL)
            return -1;
        return 0;
    }

    start = buf + buflen - 1;
    *start = '\0';

    do {
        int len;
        unsigned long name;

        kcore_readmem(cgrp + cg_name_att->offset, &name, sizeof(name));

        name += cgn_name_attr->offset;
        kcore_readmem(name, tmpname,sizeof(tmpname));

        len = strlen(tmpname);
        if ((start -= len) < buf)
            goto out;

        memcpy(start, tmpname, len);

        if (--start < buf)
            goto out;
        
        *start = '/';
        cgrp = cgp;

        kcore_readmem(cgp + cg_pa_att->offset, &cgp, sizeof(cgp));

    } while (cgp);

    ret = 0;
    memmove(buf, start, buf + buflen - start);
out:
    return ret;
}

void memcg_get_name(unsigned long memcg, char *name,
                int len, struct btf *btf_handle)
{
    char *end;
    int pos;
    unsigned long cg, knname;
    char subname[257];
    struct member_attribute *att;

    memset(subname, 0, sizeof(subname));
    att = get_offset_no_cache("mem_cgroup", "css", btf_handle);
    if (!att)
        return;
    
    cg = memcg + att->offset;

    att = get_offset_no_cache("cgroup_subsys_state", "cgroup", btf_handle);
    if (!att)
        return;

    kcore_readmem(cg + att->offset, &cg, sizeof(cg));

    if (kernel_version == LINUX_3_10) {
        if (!cg)
            return;
        cgroup_path(cg, name, PATH_MAX, btf_handle);
        end = name + strlen("sys/fs/cgroup/memory/");
        memmove(end, name, strlen(name) + 1);
        prepend(&end, &len, "sys/fs/cgroup/memory", strlen("sys/fs/cgroup/memory"), 0);
    } else {
        unsigned long kn;
        unsigned long pkn;
        int kn_name_offset, kn_pa_offset;

        att = get_offset_no_cache("cgroup", "kn", btf_handle);
        if (!att)
            return;

        kcore_readmem(cg + att->offset, &kn, sizeof(kn));

        if (!cg || !kn)
            return;

        end = name + len - 1;
        prepend(&end, &len, "\0", 1, 0);
        pkn = kn;

        kn_name_offset = get_member_offset("kernfs_node", "name", btf_handle);
        if (kn_name_offset < 0)
            return;
        
        kn_pa_offset = get_member_offset("kernfs_node", "parent", btf_handle);
        if (kn_pa_offset < 0)
            return;

        while (pkn) {
            kcore_readmem(pkn + kn_name_offset, &knname, sizeof(knname));
            kcore_readmem(knname, subname, sizeof(subname));

            pos = prepend(&end, &len, subname, strlen(subname), 0);
            if (pos)
                break;

            kcore_readmem(pkn + kn_pa_offset, &kn, sizeof(kn));
            if ((pkn == kn) || !kn)
                break;
            pos = prepend(&end, &len, "/", 1, 0);
            if (pos)
                break;
            pkn = kn;
        }

        prepend(&end, &len, "/sys/fs/cgroup/memory", strlen("/sys/fs/cgroup/memory"), 0);

        memmove(name, end, strlen(end) + 1);
    }
}