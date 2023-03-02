#ifndef __USER_API__
#define __USER_API__

//#include "common.h"
#include <linux/ioctl.h>

#define NAME_LEN (1024)
#define INODE_LIMIT (50)
struct file_item {
	char filename[NAME_LEN];
	unsigned long  size;
	unsigned long  cached;
	int  deleted;
};

struct filecache_result {
    int num;
    char fsname[NAME_LEN];
    struct file_item *filecache_items
};

struct inode_item_user {
    unsigned long i_ino;
    int nr_pages;
    int deleted:4;
    int shmem:4;
    unsigned long cached;
    char filename[NAME_LEN];
};
struct memcg_item_user {
    struct inode_item_user *inode_items;
    unsigned long  anon;
    unsigned long  shmem;
    unsigned long  file;
    unsigned long size;
    int num_file;
    char cgname[NAME_LEN];
};

struct memcg_info_user {
    int nr ;/* number of memcg for dying*/
    struct memcg_item_user* items;
    char cgname[NAME_LEN];
};

typedef enum _memhunter_type {
    MEMHUNTER_CACHE_TYPE_FILE = 1,
    MEMHUNTER_CACHE_TYPE_MEMCG_DYING,
    MEMHUNTER_CACHE_TYPE_MEMCG_ONE,
} memhunter_type;

struct memhunter_settings {
    memhunter_type type;
    int num;
    char objname[NAME_LEN];
};
#define MEMHUNTER_IO_FILE  _IOWR(2, 1, struct filecache_result)
#define MEMHUNTER_IO_MEMCG_DYING  _IOWR(2, 2, struct memcg_info_user)
#define MEMHUNTER_IO_MEMCG_ONE  _IOWR(2, 3, struct memcg_info_user)
#endif
