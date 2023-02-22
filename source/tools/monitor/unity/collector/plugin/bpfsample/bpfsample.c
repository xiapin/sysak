

#include "bpfsample.h"
#include "bpfsample.skel.h"

#include <pthread.h>
#include <coolbpf.h>
#include "../../../../unity/beeQ/beeQ.h"

DEFINE_SEKL_OBJECT(bpfsample);

int init(void *arg)
{
    printf("bpfsample plugin install.\n");
    return LOAD_SKEL_OBJECT(bpfsample);
}

int call(int t, struct unity_lines *lines)
{
    int countfd = bpf_map__fd(bpfsample->maps.count);
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
    DESTORY_SKEL_BOJECT(bpfsample);
}
