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
using namespace std;

#ifndef KPF_RESERVED
#define KPF_RESERVED 32
#endif

#ifndef KPF_IDLE
#define KPF_IDLE (25)
#endif

#define MAX_BIT (26)


map<unsigned long , struct file_info *> files;
map<unsigned long , int> inodes;
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
extern struct member_attribute *get_offset(string struct_name,  string member_name);
 
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

/*
 * pfn: page pfn
 * cinode: cgroup inode
 */ 
static int get_dentry(unsigned long pfn, unsigned long cinode, int active, int shmem, int dirty)
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
    if (cached*4 < 100)
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
    return 0;
}

unsigned long get_cgroup_inode(unsigned long pfn)
{
    unsigned long ino;
    int ret = 0;

    ret = kpagecgroup_read(&ino, sizeof(ino), pfn*sizeof(ino));
    if (ret != sizeof(ino)) {
        return 0;
    }
    return ino;
}

int check_cgroup_inode(unsigned long inode)
{
    return (full_scan||(inodes.find(inode)!=inodes.end()));
}

bool cached_cmp(const pair<unsigned long, struct file_info*>& a, const pair<unsigned long, struct file_info*>& b) {
    struct file_info *a_file = a.second;
    struct file_info *b_file = b.second;
    if (!a_file || !b_file)
        return 1;
    return a_file->cached > b_file->cached;
}

static int output_file_cached(unsigned int top)
{
    map<unsigned long,struct file_info*>::iterator iter; 
    vector< pair <unsigned long, struct file_info *> > vec(files.begin(), files.end());
    struct file_info *info;
 
    for (iter = files.begin(); iter != files.end(); ++iter) {
        info = (*iter).second;
        if (!info) {
            continue;
        }
    } 
    sort(vec.begin(), vec.end(), cached_cmp);

    for (int i = 0; i < vec.size(); ++i) {
        info = vec[i].second;
        if (!info) {
            continue;
        }
        printf("inode=%lu file=%s cached=%lu size=%lu cinode=%lu active=%lu inactive=%lu shmem=%d \ 
                delete=%d cgcached=%lu dirty=%lu\n", info->inode, info->filename, info->cached, info->size,\
                info->cinode, info->active*4, info->inactive*4, info->shmem, info->del, info->cgcached*4, info->dirty*4);
        free(info);
        if (i >= top - 1)
            break;
    }
    return 0;    
}

int scan_pageflags(struct options *opt)
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
            get_dentry(pfn, inode, active, shmem,dirty);
        } 
    }
    output_file_cached(opt->top);
    return 0;
}

int memcg_cgroup_path(const char *cgrouppath)
{
    struct stat st;
    if (access(cgrouppath, F_OK)) {
        return 0;
    }

    stat(cgrouppath, &st);
    inodes[st.st_ino] = 1;
    return 0;
}

int memcg_cgroup_file(char *cgroupfile)
{
    ifstream filename(cgroupfile);
    string cgroup;
    
    if (!filename) {
        printf("open %s failed\n", cgroupfile);
        return 0;
    }

    while (getline(filename, cgroup)) 
    {
        memcg_cgroup_path(cgroup.c_str());       
    }
    filename.close();
    return 0;
}
