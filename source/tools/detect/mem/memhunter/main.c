#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include "user_api.h"

static int error = 0;
static int off = 0;

struct item_sum
{
    int cached;
    int nr_pages;
    int deleted;
    int shmem;
};
void print_file_result(struct filecache_result* result)
{
    int i = 0;
    for(i = 0; i < result->num; i++)
        printf("\tfilename:%s size:%lu cached:%lu deleted:%d\n", result->filecache_items[i].filename, result->filecache_items[i].size, result->filecache_items[i].cached, result->filecache_items[i].deleted);
}


void print_memcg_result(struct memcg_info_user* result)
{
    struct memcg_item_user *cgitem;
    struct inode_item_user *item;
    int j, i;
    int limit = 0;
    struct item_sum sumres;
    memset(&sumres, 0, sizeof(sumres));

    for(i = 0; i < result->nr; i++)
    {
        cgitem = &result->items[i];
        printf("cg:%s memory:%lu file:%lu anon:%lu shmem:%lu num_file:%d\n", cgitem->cgname, cgitem->size, cgitem->file, cgitem->anon, cgitem->shmem, cgitem->num_file);
        limit = INODE_LIMIT > cgitem->num_file? cgitem->num_file : INODE_LIMIT;
        for(j = 0; j < limit; j++)
        {
            item = &cgitem->inode_items[j];
            if(j > limit/3 && item->cached <= 1 && (sumres.cached != 0 || j!= limit-1))
            {
               sumres.cached += item->cached;
               sumres.nr_pages += item->nr_pages;
               sumres.deleted += item->deleted;
               sumres.shmem += item->shmem;
               continue;
            }
            printf("\tino:%lu, filename:%s cached:%lu nr_pages:%d deleted:%d shmem:%d\n", item->i_ino, item->filename, item->cached, item->nr_pages, item->deleted, item->shmem);
        }
        if(sumres.cached != 0)
            printf("\tothers total cached:%lu nr_pages:%d deleted:%d shmem:%d\n", sumres.cached, sumres.nr_pages, sumres.deleted, sumres.shmem);
        memset(&sumres, 0, sizeof(sumres));
    }

}

static void show_usage(void)
{
    printf("-c [cgroup_name] get specific memcg cache \n");
    printf("-d   get leak memcg memory usage\n");
    printf("-f scan tmpfs & ext4 file system cache\n");
    printf("-n: # of output\n");

}

int get_arg(struct memhunter_settings *set, int argc, char * argv[])
{
    int ch;

    while ((ch = getopt(argc, argv, "fc:n:dh")) != -1)
    {
        switch (ch)
        {
            case 'f':
                set->type = MEMHUNTER_CACHE_TYPE_FILE;
                break;
            case 'd':
                set->type = MEMHUNTER_CACHE_TYPE_MEMCG_DYING;
                break;
            case 'c':
                strcpy(set->objname, optarg);
                set->type = MEMHUNTER_CACHE_TYPE_MEMCG_ONE;
                break;
            case 'n':
                set->num = atoi(optarg);
                break;
            case 'h':
                show_usage();
                error = 1;
                break;
            case '?':
                printf("Unknown option: %c\n",(char)optopt);
                error = 1;
                break;
        }
    }
}

int init_file_result(struct filecache_result *file_result, int num)
{
    file_result->num = num;
    //printf("num:%d, size:%d\n", file_result->num, sizeof(struct file_item));
    file_result->filecache_items = malloc(file_result->num * sizeof(struct file_item));
    if(!file_result->filecache_items)
    {
        printf("get file result error\n");
        return -1;
    }
    memset(file_result->filecache_items, 0, file_result->num * sizeof(struct file_item));
    return 0;
}
int init_memcg_result(struct memcg_info_user *meresult, int num)
{
    int i = 0;
    meresult->nr = num;
    meresult->items = malloc(sizeof(struct memcg_item_user) * meresult->nr);
    if(!meresult->items)
    {
         printf("get memcg result error\n");
         return -1;
    }

    memset(meresult->items, 0, sizeof(struct memcg_item_user) * meresult->nr);
    for(i = 0; i < meresult->nr; i++)
    {
        meresult->items[i].inode_items = malloc(INODE_LIMIT * sizeof(struct inode_item_user));
        memset(meresult->items[i].inode_items, 0, INODE_LIMIT * sizeof(struct inode_item_user));
        if(!meresult->items[i].inode_items)
        {
            printf("get memcg result error\n");
            free_memcg_result(meresult);
            return -1;
        }
    }
    return 0;

}
void free_memcg_result(struct memcg_info_user *meresult)
{
    int i = 0;
    for(i = 0; i < meresult->nr; i++)
    {
        if(meresult->items[i].inode_items)
            free(meresult->items[i].inode_items);
    }
    free(meresult->items);
}
int main(int argc, char **argv)
{
    struct filecache_result file_result;
    struct memcg_info_user meresult;
    struct memhunter_settings set;
    int ret = 0, fd = 0;

    memset(&set, 0, sizeof(set));
    memset(&meresult, 0, sizeof(meresult));
    memset(&file_result, 0, sizeof(file_result));

    get_arg(&set, argc, argv);

    if (error)
        return 0;

    fd = open("/dev/sysak", O_RDWR);
    if (fd < 0) {
        printf("open mehunter check error\n");
        return -1;
    }

    set.num = set.num > 0 ? set.num : 1;
    switch (set.type) {

        case MEMHUNTER_CACHE_TYPE_FILE:
            ret = init_file_result(&file_result, set.num);
            strncpy(file_result.fsname, "ext4", strlen("ext4"));
            if(ret)
                goto _out;
            ret = ioctl(fd, MEMHUNTER_IO_FILE, &file_result);
            if (ret) {
                printf("ioctl error %s \n", strerror(ret));
                if(file_result.filecache_items)
                    free(file_result.filecache_items);
                goto _out;
            }
            printf("file cache scan result of ext4:\n");
            print_file_result(&file_result);

            memset(file_result.filecache_items, 0, file_result.num * sizeof(struct file_item));
            memset(file_result.fsname, 0, sizeof(file_result.fsname));
            strncpy(file_result.fsname, "tmpfs", strlen("tmpfs"));
            ret = ioctl(fd, MEMHUNTER_IO_FILE, &file_result);
            if (ret) {
                printf("ioctl error %s \n", strerror(ret));
                if(file_result.filecache_items)
                    free(file_result.filecache_items);
                goto _out;
            }
            printf("file cache scan result of tmpfs:\n");
            print_file_result(&file_result);


            if(file_result.filecache_items)
                free(file_result.filecache_items);
            break;

        case MEMHUNTER_CACHE_TYPE_MEMCG_DYING:
            ret = init_memcg_result(&meresult, set.num);
            if(ret)
                goto _out;
            strncpy(meresult.cgname, set.objname, strlen(set.objname));
            ret = ioctl(fd, MEMHUNTER_IO_MEMCG_DYING, &meresult);
            if (ret) {
                printf("ioctl error %s \n", strerror(ret));
                free_memcg_result(&meresult);
                goto _out;
            }
            printf("dying memcg scan result:\n");
            print_memcg_result(&meresult);
            free_memcg_result(&meresult);
            break;

        case MEMHUNTER_CACHE_TYPE_MEMCG_ONE:
            set.num = 1;
            ret = init_memcg_result(&meresult, set.num);
            if(strlen(set.objname) == 0)
            {
                printf("please use -o to assign cgroup name");
                goto _out;
            }
            strncpy(meresult.cgname, set.objname, strlen(set.objname));
            //printf("name len:%d\n",strlen(meresult.cgname));
            printf("cgname:%s\n",meresult.cgname);
            if(ret)
                goto _out;
            ret = ioctl(fd, MEMHUNTER_IO_MEMCG_ONE, &meresult);
            if (ret) {
                printf("ioctl error %s \n", strerror(ret));
                free_memcg_result(&meresult);
                goto _out;
            }
            printf("memcg scan result:\n");
            print_memcg_result(&meresult);
            free_memcg_result(&meresult);
            break;
        default:
            printf("invalid input\n");
    };
_out:
    close(fd);
    return 0;
}
