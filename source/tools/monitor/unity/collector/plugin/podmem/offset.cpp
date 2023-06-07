#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
extern "C" {
#include "btfparse.h"
}

#include <map>
#include <string>
using namespace std;
struct btf * handle;
#define LEN (128)

map <string, struct member_attribute *> struct_offset;

int sym_init(char *btf_name)
{
    handle = btf_load(btf_name);

    return !!(handle == NULL);
}

int sym_uninit(void)
{
    btf__free(handle);
    map<string,struct member_attribute *>::iterator iter; 
    struct member_attribute *info;
 
    for (iter = struct_offset.begin(); iter != struct_offset.end(); ++iter) {
        info = (*iter).second;
        if (info) {
            free(info);
            (*iter).second = NULL;
        }   
    }   
    return 0; 
}

struct member_attribute *get_offset(string struct_name,  string member_name)
{
    string index;
    struct member_attribute *att;
    map<string,struct member_attribute*>::iterator iter;

    index = struct_name + "_" + member_name;
    iter = struct_offset.find(index);
    if (iter != struct_offset.end()) {
        return iter->second;
    } 
    att = btf_find_struct_member(handle, (char*)struct_name.c_str(), (char*)member_name.c_str());
    if (!att) {
        //printf("get %s error \n", index.c_str());
        return NULL;
    }
    //printf("%s:offset:%d, size:%d\n", index.c_str(),att->offset, att->size);
    att->offset = att->offset/8;
    struct_offset[index] = att;          
    return att;
}

void stripWhiteSpace (string &str)
{
    if (str == "")
    {
        return;
    }
 
    string::iterator cur_it;
    cur_it = str.begin();
 
    while (cur_it != str.end())
    {
        if (((*cur_it) != '\t') && ((*cur_it) != ' '))
        {
            break;
        }
        else
        {
            cur_it = str.erase (cur_it);
        }
    }
 
    cur_it = str.begin();
 
    while (cur_it != str.end())
    {
        if ((*cur_it) == '\n')
        {
            cur_it = str.erase (cur_it);
        }
        else
        {
            cur_it++;
        }
    }
}

static int do_cmd(const char *cmd, char *result, int len)
{
    FILE *res;
    char region[LEN] = {0};
    string str;

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
    strncpy(result, str.c_str(), len - 1);
    result[len - 1] = '\0';
    pclose(res);
    return 0;
}

static int download_btf(void)
{
    char region[LEN] = {0};
    char arch[LEN] = {0};
    char kernel[LEN] = {0};
    char dw[LEN+LEN] = {0};
    string sysak_path = "/boot";
    string timeout = "-internal";
    string cmd = "curl -s --connect-timeout 2 http://100.100.100.200/latest/meta-data/region-id 2>&1";

    do_cmd(cmd.c_str(), region, LEN);
    if (!strstr(region,"cn-")) {
        strcpy(region, "cn-hangzhou");
        timeout = "";
    }
    //printf("region:%s\n", region);
    cmd = "uname -m";
    do_cmd(cmd.c_str(), arch, LEN);
    //printf("arch:%s\n", arch);

    cmd = "uname -r";
    do_cmd(cmd.c_str(), kernel, LEN);
    //printf("kernel:%s\n", kernel);

    if(getenv("SYSAK_WORK_PATH") != NULL)
    {
        sysak_path = getenv("SYSAK_WORK_PATH") ;
        sysak_path += "/tools/";
        sysak_path += kernel;
    }

    snprintf(dw, LEN + LEN + LEN, "wget -T 5 -t 2 -q -O %s/vmlinux-%s https://sysom-cn-%s.oss-cn-%s%s.aliyuncs.com/home/hive/btf/%s/vmlinux-%s",sysak_path.c_str(),  kernel, &region[3],&region[3],timeout.c_str(),arch, kernel);

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

int offset_init(void)
{
    int ret = 0;
    char btf[LEN] = {0};
    char ver[LEN] = {0};
    const char *cmd;

    cmd = string("uname -r").c_str();
    do_cmd(cmd, ver, LEN);
    if(getenv("SYSAK_WORK_PATH") != NULL)
        sprintf(btf,"%s/tools/%s/vmlinux-%s", getenv("SYSAK_WORK_PATH"), ver, ver);
    else
        sprintf(btf,"/boot/vmlinux-%s", ver);

    if (check_btf_file(btf)) {
        download_btf();
    };

    if (check_btf_file(btf)) {
        printf("btf file:%s not found \n", btf);
        return -1;
    }
    ret = sym_init(btf);
    if (!!ret) {
        printf("get sym init error\n");
        return -1;
    } 
    
    get_offset("page", "mapping");
    get_offset("address_space", "host");
    get_offset("address_space", "nrpages");
    get_offset("inode", "i_ino");
    get_offset("inode", "i_size");
    get_offset("inode", "i_sb");
    get_offset("inode", "i_dentry");
    get_offset("dentry", "d_alias");
    get_offset("dentry", "d_parent");
    get_offset("dentry", "d_hash");
    get_offset("dentry", "d_name");
    get_offset("dentry", "d_name");
    get_offset("super_block", "s_mounts");
    get_offset("mount", "mnt_instance");
    get_offset("mount", "mnt_parent");
    get_offset("mount", "mnt_mountpoint");
    get_offset("mount", "mnt_mountpoint");

    return 0;
}

