

#include "bpfsample.h"
#include "bpfsample.skel.h"

#include <pthread.h>
#include <coolbpf.h>
#include "../../../../unity/beeQ/beeQ.h"

struct coolbpf_object *cb = NULL;
int countfd = 0;

int init(void *arg)
{
    cb = coolbpf_object_new(bpfsample);
    if (!cb) {
        printf("Failed to create coolbpf object\n");
        return -EINVAL;
    }
    
    countfd = coolbpf_object_find_map(cb, "count");
    if (countfd < 0) {
        printf("Failed to get count map fd\n");
        return countfd;
    }
    printf("bpfsample plugin install.\n");
    printf("count map fd is %d\n", countfd);
    return 0;
}

int call(int t, struct unity_lines *lines)
{
    int default_key = 0;
    uint64_t count = 0;
    uint64_t default_count = 0;
    struct unity_line* line;

    bpf_map_lookup_elem(countfd, &default_key, &count);
    bpf_map_update_elem(countfd, &default_key, &default_count, BPF_ANY);

    unity_alloc_lines(lines, 1); 
    line = unity_get_line(lines, 0);
    unity_set_table(line, "bpfsample");
    unity_set_value(line, 0, "value", count);

    return 0;
}

void deinit(void)
{
    printf("bpfsample plugin uninstall.\n");
    coolbpf_object_destroy(cb);
}
