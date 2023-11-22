#include <linux/kernel-page-flags.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "cache.h"
#include "memcg.h"
#include "btfparse.h"

#include<iostream>
#include<istream>
#include<cstring>
#include<string>
#include<fstream>
#include<map>
#include<vector>
#include<algorithm>
#include<set>
using namespace std;

#ifndef KPF_RESERVED
#define KPF_RESERVED 32
#endif

#ifndef KPF_IDLE
#define KPF_IDLE (25)
#endif

#define MAX_BIT (26)


static bool full_scan;
static unsigned int scan_rate = 4;

struct file_info {
    char filename[256];
    unsigned long  inode;
    unsigned long cached;
    unsigned long cgcached;
    unsigned long size;
    unsigned long active;
    unsigned long dirty;
    unsigned long inactive;
    unsigned long cinode;
    int shmem;
    int del;
};
struct myComp2
{
    bool operator()(const pair<unsigned long,int> &a,const pair<unsigned long,int> &b)
    {
        return a.second >= b.second;
    }
};

//set<pair<unsigned long, int> ,myComp2> cachedset;
// cachedset[cgroup_inode] = set<pair<file_inode, cached_size>>
map<unsigned long, set<pair<unsigned long, int>, myComp2>* > cinode_cached;
map<unsigned long , struct file_info *> files;
struct myComp
{
    bool operator()(const unsigned long &a,const unsigned long &b)
    {
        return files[a]->cached > files[b]->cached || (files[a]->cached == files[b]->cached && a != b);
    }
};
set<unsigned long,myComp> fileset;
//map<unsigned long , int> inodes;
map<unsigned long , int> history_inodes;
// memcg -> inode cache
map<unsigned long , unsigned long> memcg_inodes;

extern int kpagecgroup_fd;
extern struct member_attribute *get_offset(string struct_name,  string member_name);
extern int get_structsize(char *type_name);
 
static int prepend(char **buffer, int *buflen, const char *str, int namelen, int off)
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

static unsigned long inode2mount(unsigned long inode)
{
    unsigned long sb;
    unsigned long mount;
    struct member_attribute *att;
    att = get_offset("inode", "i_sb");
    if (!att) {
        return 0;
    } 
    kcore_readmem(inode + att->offset, &sb, sizeof(sb));
    att = get_offset("super_block", "s_mounts");
    if (!att) {
        return 0;
    }
    kcore_readmem(sb+ att->offset, &mount, sizeof(mount));
    att = get_offset("mount", "mnt_instance");
    if (!att) {
        return 0;
    }
    mount -= att->offset; 
    return mount; 
}

static int get_filename(unsigned long dentry, char *filename, int len)
{
    unsigned long parent;
    char name[4096] = {0};
    char tmp[4096] = {0};
    char *end = tmp + 4095;
    int buflen = 4095;
    struct qstr str;
     struct member_attribute *att;
    while (dentry) {
        
        memset(name, 0, 128);
        att = get_offset("dentry", "d_parent");
        kcore_readmem(dentry+att->offset, &parent, sizeof(parent));
        att = get_offset("dentry", "d_name");
        if (!att) {
            return 0;
        }
        kcore_readmem(dentry + att->offset, &str, sizeof(str));
        if (parent == dentry || str.len<=0)
            break;    
        dentry = parent;
        if (str.len > 4096)
            str.len = 4096;

        kcore_readmem((unsigned long)str.name, name, str.len); 
        prepend(&end, &buflen, name, strlen(name), 1); 
    }
    strncpy(filename, end, len);
    return 0;
}

int get_top_dentry(unsigned long pfn, int top, unsigned long cinode)
{
    unsigned long page = PFN_TO_PAGE(pfn);
    map<unsigned long,struct file_info*>::iterator iter2;
    map<unsigned long, set<pair<unsigned long, int>, myComp2>* >::iterator it;
    set<pair<unsigned long, int>, myComp2> *cachedset;
    struct member_attribute *att;
    struct member_attribute *att2;
    unsigned long map = 0;
    unsigned long cached = 0;
    unsigned long inode = 0;
    unsigned long i_ino;
    char* tables;

    it = cinode_cached.find(cinode);
    if (it != cinode_cached.end()) {
        cachedset = it->second;
    } else {
        return 0;
    }

    att = get_offset("page", "mapping");
    if (!att) {
        return 0;
    }

    kcore_readmem(page + att->offset, &map, sizeof(map));
    if (!is_kvaddr(map))
        return 0;
    att = get_offset("address_space", "host");
    if (!att) {
        return 0;
    }
    att2 = get_offset("address_space", "nrpages");
    if (!att2) {
        return 0;
    }
    tables = (char*)malloc(att2->offset-att->offset + sizeof(cached));
    kcore_readmem(map + att->offset, tables, att2->offset-att->offset + sizeof(cached));
    inode = *((unsigned long*) tables);
    cached = *((unsigned long*) (tables+att2->offset-att->offset));
    free(tables);

    /* skip file cache < 1M */
    if (cached*4 < 1024)
        return 0;
    
    if (history_inodes.find(inode) != history_inodes.end() or (cachedset->size() >= top and (cached*4 < (--cachedset->end())->second)))
        return 0;
    
    //printf("---after filter---: cached:%d, cinode:%ld, inode:%lu\n", cached*4, cinode, inode);
    cachedset->insert(pair<unsigned long, int>(inode, cached*4));
    history_inodes[inode] = cinode;
    if (cachedset->size() > top) {
        set<pair<unsigned long , int>, myComp2>::iterator iter;
        iter = --cachedset->end();
        //printf("erase inode size: %d\n", iter->second);
        cachedset->erase(--cachedset->end());
    }
    
    cinode_cached[cinode] = cachedset;
}

static int iterate_dentry_path()
{
    set<pair<unsigned long , int>, myComp2>::iterator iter;
    map<unsigned long, set<pair<unsigned long, int>, myComp2>* >::iterator it;
    unsigned long map = 0;
    unsigned long inode = 0;
    unsigned long i_ino;
    unsigned long i_size;
    unsigned long inode_dentry =0;
    unsigned long dentry_first = 0;
    unsigned long hdentry = 0;
    unsigned long pdentry = 0;
    unsigned long mount = 0;
    unsigned long cached;
    int del = 0;
    struct file_info *info;
    struct member_attribute *att;
    for (it = cinode_cached.begin(); it != cinode_cached.end(); ++it) {
        set<pair<unsigned long , int>, myComp2> *cachedset = it->second;
        for (iter = cachedset->begin(); iter != cachedset->end(); iter++) {
            char tmp[4096] = {0};
            char *end = tmp + 4095;
            int buflen = 4095;
            char filename[1024] = {0};
            cached = iter->second;
            inode = iter->first;
            att = get_offset("inode", "i_ino");
            if (!att) {
                return 0;
            }
            kcore_readmem(inode + att->offset, &i_ino, sizeof(i_ino));
            att = get_offset("inode", "i_size");
            if (!att) {
                return 0;
            }
            kcore_readmem(inode + att->offset, &i_size, sizeof(i_size));

            mount = inode2mount(inode);
            att = get_offset("inode","i_dentry");
            if (!att) {
                return 0;
            }
            kcore_readmem(inode + att->offset, &inode_dentry, sizeof(inode));
            if (!is_kvaddr(inode_dentry))
                continue;
            att = get_offset("dentry", "d_alias");
            if (!att) {
                att = get_offset("dentry", "d_u");
                if (!att)
                    return 0;
            }

            dentry_first = inode_dentry - att->offset;
            memset(filename, 0, 1024);
            att = get_offset("dentry", "d_parent");
            if (!att) {
                return 0;
            }
            kcore_readmem(dentry_first+att->offset, &pdentry, sizeof(pdentry));
            att = get_offset("dentry", "d_hash");
            if (!att) {
                return 0;
            }
            kcore_readmem(dentry_first+att->offset + sizeof(void*), &hdentry, sizeof(hdentry));
            if ((dentry_first != pdentry) && !hdentry)
                del = 1;
            do {
                unsigned long mount_parent = 0;
                unsigned long mount_dentry = 0;
                int len = 0;
                int ret = 0;

                get_filename(dentry_first, filename, 1024);
                len = strlen(filename);
                if (len <=0 || ((len == 1) && (filename[0] == '/')))
                    break;

                prepend(&end, &buflen, filename, strlen(filename), 0);
                att = get_offset("mount", "mnt_parent");
                if (!att) {
                    return 0;
                }
                ret = kcore_readmem(mount + att->offset, &mount_parent , sizeof(mount_parent));
                if (ret != sizeof(mount_parent))
                    break;
                att = get_offset("mount", "mnt_mountpoint");
                if (!att) {
                    return 0;
                }
                kcore_readmem(mount+ att->offset, &mount_dentry, sizeof(mount_dentry));
                if (mount_parent == mount || mount_dentry==dentry_first)
                    break;
                dentry_first = mount_dentry;
                mount = mount_parent;
            } while(true);

            if (buflen >= 4092)
                continue;
            info = (struct file_info *)malloc(sizeof(struct file_info));
            if (!info) {
                printf("alloc file info error \n");
                continue;
            }
            memset(info, 0, sizeof(struct file_info));
            info->inode = i_ino;
            //info->shmem = shmem;
            info->cached = cached;
            info->cgcached = 1;
            info->active = 0;
            info->dirty = 0;
            info->inactive = 0;
            info->del = del;
            info->cinode = history_inodes[inode];

            info->size = i_size>>10;
            strncpy(info->filename, end, sizeof(info->filename) - 2);
            info->filename[sizeof(info->filename) -1] = '0';
            files[i_ino] = info;
            fileset.insert(i_ino);
        }
    }
    return 0;
}

/*
 * pfn: page pfn
 * cinode: cgroup inode
 */ 
static int get_dentry(unsigned long pfn, unsigned long cinode, int active, int shmem, int dirty, int top)
{
    map<unsigned long,struct file_info*>::iterator iter;
    unsigned long page = PFN_TO_PAGE(pfn);
    unsigned long map = 0;
    unsigned long cached = 0;
    unsigned long inode = 0;
    unsigned long inode_dentry =0;
    unsigned long dentry_first = 0;
    unsigned long hdentry = 0;
    unsigned long pdentry = 0;
    unsigned long mount = 0;
    char filename[1024] = {0};
    char tmp[4096] = {0};
    char *end = tmp + 4095;
    int buflen = 4095;
    unsigned long i_ino;
    unsigned long i_size;
    struct file_info *info;
    int del = 0;
    struct member_attribute *att;
    att = get_offset("page", "mapping");
    if (!att) {
        return 0;
    }

    kcore_readmem(page + att->offset, &map, sizeof(map));
    if (!is_kvaddr(map))
        return 0;
    att = get_offset("address_space", "nrpages");
    if (!att) {
        return 0;
    }
    kcore_readmem(map + att->offset,  &cached, sizeof(cached));
    /* skip file cache < 100K */ 
    if (fileset.size() >= top and (cached*4 < (files[*(--fileset.end())]->cached)))
        return 0;
    
    att = get_offset("address_space", "host");
    if (!att) {
        return 0;
    }
    kcore_readmem(map + att->offset, &inode, sizeof(inode));
    att = get_offset("inode", "i_ino");
    if (!att) {
        return 0;
    }
    kcore_readmem(inode + att->offset, &i_ino, sizeof(i_ino)); 
    iter = files.find(i_ino);    
    if (iter != files.end()) {
        info = iter->second;
        if (active) {
            info->active += 1;
        }else {
            info->inactive += 1;
        }
        if (dirty) {
            info->dirty += 1;
        }

        if (info->cinode == cinode)
            info->cgcached++;
        return 0; 
    }
    att = get_offset("inode", "i_size");
    if (!att) {
        return 0;
    }
    kcore_readmem(inode + att->offset, &i_size, sizeof(i_size)); 

    mount = inode2mount(inode);
    att = get_offset("inode","i_dentry");
    if (!att) {
        return 0;
    }
    kcore_readmem(inode + att->offset, &inode_dentry, sizeof(inode));
    if (!is_kvaddr(inode_dentry))
        return 0;
    att = get_offset("dentry", "d_alias");
    if (!att) {
        att = get_offset("dentry", "d_u");
        if (!att)
            return 0;
    }

    dentry_first = inode_dentry - att->offset;
    memset(filename, 0, 1024);
    att = get_offset("dentry", "d_parent");
    if (!att) {
        return 0;
    }
    kcore_readmem(dentry_first+att->offset, &pdentry, sizeof(pdentry));
    att = get_offset("dentry", "d_hash");
    if (!att) {
        return 0;
    }
    kcore_readmem(dentry_first+att->offset + sizeof(void*), &hdentry, sizeof(hdentry));
    if ((dentry_first != pdentry) && !hdentry)
        del = 1;
    do {
        unsigned long mount_parent = 0;
        unsigned long mount_dentry = 0;
        int len = 0;
        int ret = 0;

        get_filename(dentry_first, filename, 1024);
        len = strlen(filename); 
        if (len <=0 || ((len == 1) && (filename[0] == '/')))
            break;

        prepend(&end, &buflen, filename, strlen(filename), 0); 
        att = get_offset("mount", "mnt_parent");
        if (!att) {
            return 0;
        } 
        ret = kcore_readmem(mount + att->offset, &mount_parent , sizeof(mount_parent));
        if (ret != sizeof(mount_parent))
            break;
        att = get_offset("mount", "mnt_mountpoint");
        if (!att) {
            return 0;
        }
        kcore_readmem(mount+ att->offset, &mount_dentry, sizeof(mount_dentry));  
        if (mount_parent == mount || mount_dentry==dentry_first)
            break;
        dentry_first = mount_dentry;
        mount = mount_parent;
     } while(true);

    if (buflen >= 4092)
        return 0;
    info = (struct file_info *)malloc(sizeof(struct file_info));
    if (!info) {
        printf("alloc file info error \n");
        return 0;
    }
    memset(info, 0, sizeof(sizeof(struct file_info)));
    info->inode = i_ino;
    info->shmem = shmem;
    info->cached = cached*4;
    info->cgcached = 1;
    info->active = 0;
    info->dirty = 0;
    info->inactive = 0;
    info->del = del;
    if (active)
        info->active = 1;
    else
        info->inactive = 1;
    if (dirty)
        info->dirty = 1;

    info->cinode = cinode;
    info->size = i_size>>10;
    strncpy(info->filename, end, sizeof(info->filename) - 2);
    info->filename[sizeof(info->filename) -1] = '0';
    files[i_ino] = info;
    fileset.insert(i_ino);
    if(fileset.size() > top)
    {
        i_ino = *(--fileset.end());
        fileset.erase(--fileset.end());
        iter=files.find(i_ino);
        free(iter->second);
        files.erase(iter);
    }
    return 0;
}

/*
 * struct mem_section *mem_section[NR_SECTION_ROOTS]
 */
static unsigned long __nr_to_section(unsigned long nr)
{   
    unsigned long offset = nr & SECTION_ROOT_MASK;
    unsigned long mem_sections;

    // mem_sections = mem_section[i]
    kcore_readmem(mem_section_base + SECTION_NR_TO_ROOT(nr) * sizeof(unsigned long), 
        &mem_sections, sizeof(mem_sections));
    
    // mem_section = mem_sections[i]
    return mem_sections + offset * mem_section_size;
}

static unsigned long __pfn_to_section(unsigned long pfn)
{
	return __nr_to_section(pfn_to_section_nr(pfn));
}

// pfn to page_cgroup
unsigned long lookup_page_cgroup(unsigned long pfn)
{
    unsigned long mem_section_addr = __pfn_to_section(pfn);
    //LOG_WARN("mem_section_addr: %lx\n", mem_section_addr);  
    unsigned long mem_section;
    unsigned long page_cgroup;
    int page_cgroup_size;
    struct member_attribute *mem_sec_att;

    mem_sec_att = get_offset("mem_section", "page_cgroup");
    if (!mem_sec_att) {
        return 0;
    }

    kcore_readmem(mem_section_addr + mem_sec_att->offset, &page_cgroup, sizeof(page_cgroup));
    page_cgroup_size = get_structsize("page_cgroup");

    return page_cgroup + pfn * page_cgroup_size;
}

// page_cgroup to mem_cgroup
unsigned long mem_cgroup_from_page_group(unsigned long page_cgroup)
{
    unsigned long mem_cgroup;
    struct member_attribute *memcg_att;

    memcg_att = get_offset("page_cgroup", "mem_cgroup");
    if (!memcg_att) {
        return 0;
    }

    kcore_readmem(page_cgroup + memcg_att->offset, 
        &mem_cgroup, sizeof(mem_cgroup));

    return mem_cgroup;
}

unsigned long page_cgroup_ino(unsigned long pfn)
{   
    unsigned long page_group, mem_cgroup;
    unsigned long css, cgroup, dentry, inode, i_ino;
    struct member_attribute *memcg_css_att, *css_cg_att;
    struct member_attribute *cg_dentry_att, *dentry_inode_att;
    struct member_attribute *inode_i_ino_att;
    map<unsigned long, unsigned long>::iterator iter;

    page_group = lookup_page_cgroup(pfn);
    if (!page_group) {
        return 0;
    }

    mem_cgroup = mem_cgroup_from_page_group(page_group);
    if (!mem_cgroup) {
        return 0;
    }

    // check memcg -> inode cache
    iter = memcg_inodes.find(mem_cgroup);
    if (iter != memcg_inodes.end()) {
        return iter->second;
    }

    /* memcg->css.cgroup->dentry->d_inode->i_ino;*/
    memcg_css_att = get_offset("mem_cgroup", "css");
    if (!memcg_css_att) {
        return 0;
    }

    css_cg_att = get_offset("cgroup_subsys_state", "cgroup");
    if (!css_cg_att) {
        return 0;
    }

    // css = mem_cgroup(addr) + css_offset
    css = mem_cgroup + memcg_css_att->offset;

    // cgroup = readmem(css + cgroup_offset)
    kcore_readmem(css + css_cg_att->offset, 
        &cgroup, sizeof(cgroup));

    cg_dentry_att = get_offset("cgroup", "dentry");
    if (!cg_dentry_att) {
        return 0;
    }

    // dentry = readmem(cgroup + dentry_offset)
    kcore_readmem(cgroup + cg_dentry_att->offset, 
        &dentry, sizeof(dentry));
    
    dentry_inode_att = get_offset("dentry", "d_inode");
    if (!dentry_inode_att) {
        return 0;
    }

    // inode = readmem(dentry + inode_offset)
    kcore_readmem(dentry + dentry_inode_att->offset, 
        &inode, sizeof(inode));
    
    inode_i_ino_att = get_offset("inode", "i_ino");
    if (!inode_i_ino_att) {
        return 0;
    }

    //  i_ino = readmem(inode + i_ino_offset)
    kcore_readmem(inode + inode_i_ino_att->offset, 
        &i_ino, sizeof(i_ino));

    memcg_inodes[mem_cgroup] = i_ino;
    
    return i_ino;
}

unsigned long get_cgroup_inode(unsigned long pfn)
{
    unsigned long ino;
    int ret = 0;

    if (kpagecgroup_fd > 0) {
        ret = kpagecgroup_read(&ino, sizeof(ino), pfn*sizeof(ino));
        if (ret != sizeof(ino)) {
            return 0;
        }
    }
    else {
        ino = page_cgroup_ino(pfn);
    }

    return ino;
}

int check_cgroup_inode(unsigned long inode)
{
    return (full_scan||(cinode_cached.find(inode)!=cinode_cached.end()));
}

bool cached_cmp(const pair<unsigned long, struct file_info*>& a, const pair<unsigned long, struct file_info*>& b) {
    struct file_info *a_file = a.second;
    struct file_info *b_file = b.second;
    if (!a_file || !b_file)
        return 1;
    return a_file->cached > b_file->cached;
}

static int output_file_cached_string(unsigned int top, char *res)
{   
    map<unsigned long, set<pair<unsigned long, int>, myComp2>* >::iterator it;
    set<unsigned long, myComp>::iterator  iter2;
    struct file_info *info;
    int size = 0;

    for (iter2 = fileset.begin(); iter2 != fileset.end(); ++iter2) {
        info = files[*iter2];
        if (!info) {
            continue;
        }
        size += sprintf(res + size, "cinode=%lu cached=%lu size=%lu file=%s\n", info->cinode,info->cached, info->size,info->filename);
        free(info);
        files[*iter2] = NULL;
    }

    for (it = cinode_cached.begin(); it != cinode_cached.end(); ++it) {
        delete it->second;
    }

    files.clear();
    fileset.clear();
    cinode_cached.clear();
    history_inodes.clear();

    return 0;
}

int scan_pageflags_nooutput(struct options *opt, char *res)
{
    unsigned long  pageflag;
    unsigned long pfn = 0;
    unsigned long inode = 0;
    int active = 0; 
    int dirty = 0;
    int shmem = 0;

    if (opt->rate != 0) {
        scan_rate = opt->rate;
    } 
    full_scan = opt->fullscan;
    while (1) {
        int ret = 0;
        pageflag = 0;
        
        pfn += scan_rate ;/* skip 2M*/
        
        if (pfn > max_pfn)
            break;
        ret = kpageflags_read(&pageflag, sizeof(pageflag), sizeof(pageflag)*pfn); 
        if (ret != sizeof(pageflag)) {
            break;
        }
        if (pageflag & (1 << KPF_NOPAGE) || !pageflag)
            continue;
    
        if ((pageflag & (1<<KPF_BUDDY)) || (pageflag & (1 << KPF_IDLE))) 
            continue;
    
        if ((pageflag & (1<<KPF_SLAB)) || ((pageflag >> KPF_RESERVED) & 0x1))
            continue;
        if (pageflag & (1 << KPF_ANON))
            continue;
 
        active = !!((1<<KPF_ACTIVE) & pageflag); 
        dirty = !!((1<<KPF_DIRTY) & pageflag);
        shmem = !!(pageflag & (1 <<KPF_SWAPBACKED)); 
        inode = get_cgroup_inode(pfn);
        if (check_cgroup_inode(inode)) {
            get_top_dentry(pfn, opt->top, inode);
        } 
    }
    iterate_dentry_path();
    output_file_cached_string(opt->top, res);
    return 0;
}

int memcg_cgroup_path(const char *cgrouppath)
{
    struct stat st;
    set<pair<unsigned long, int>, myComp2>* cachedset = new set<pair<unsigned long, int>, myComp2>;
    if (access(cgrouppath, F_OK)) {
        return 0;
    }

    stat(cgrouppath, &st);
    cinode_cached[st.st_ino] = cachedset;
    return 0;
}

int memcg_cgroup_file(const char *cgroupfile)
{
    ifstream filename(cgroupfile);
    string cgroup;
    int count = 0;
    
    if (!filename) {
        printf("open %s failed\n", cgroupfile);
        return 0;
    }

    while (getline(filename, cgroup)) 
    {
        memcg_cgroup_path(cgroup.c_str());       
        count ++;
    }
    filename.close();
    return count;
}
