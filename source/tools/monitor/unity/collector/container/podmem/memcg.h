#ifndef __POD__
#define __POD__

#include<iostream>
#include<cstring>
#include<map>
using namespace std;

#define SIZE (32)
#define NAME (4096)

/*file */
struct fileinfo {
    char name[NAME];
    unsigned long cached;
    unsigned long size;
    unsigned long inode;
    unsigned long ino;
};
typedef unsigned int u32;
typedef unsigned long long u64;

struct qstr {
    union {
        struct {
            u32 hash;
            u32 len;
        };  
        u64 hash_len;
    };  
    const unsigned char *name;
};
#endif
