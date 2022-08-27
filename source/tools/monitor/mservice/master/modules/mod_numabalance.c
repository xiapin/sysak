#include "tsar.h"

struct stats_numabalance {
    unsigned long long numa_hint_faults;
    unsigned long long numa_hint_faults_local;
    unsigned long long numa_pages_migrated;
};

#define NR_EVENT 3
#define STATS_PGEVENT_SIZE       (sizeof(struct stats_numabalance))

static char *numabalance_storage = "    --numabalance             numa balance event";

/*
 *************************************************************
 * Read vmevent statistics from /proc/vmstat.
 *************************************************************
 */
static void
read_vmstat_numabalance(struct module *mod)
{
    FILE              *fp;
    char               line[LEN_4096], buf[LEN_4096];
    struct stats_numabalance  st_numabalance;

    memset(buf, 0, LEN_4096);
    memset(&st_numabalance, 0, sizeof(struct stats_numabalance));
    /* read /proc/vmstat*/
    if ((fp = fopen(VMSTAT, "r")) == NULL) {
        return ;
    }

    while (fgets(line, LEN_4096, fp) != NULL) {

        if (!strncmp(line, "numa_hint_faults ", 17)) {
            sscanf(line + 17, "%llu", &st_numabalance.numa_hint_faults);

        } else if (!strncmp(line, "numa_hint_faults_local ", 23)) {
            sscanf(line + 23, "%llu", &st_numabalance.numa_hint_faults_local);

        } else if (!strncmp(line, "numa_pages_migrated ", 20)) {
            sscanf(line + 20, "%llu", &st_numabalance.numa_hint_faults_local);

        } 
    }
    fclose(fp);
    sprintf(buf, "%lld,%lld,%lld", st_numabalance.numa_hint_faults,
            st_numabalance.numa_hint_faults_local, st_numabalance.numa_hint_faults_local);
    set_mod_record(mod, buf);
    return;
}

static void
set_numabalance_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < NR_EVENT; i++) {
        st_array[i] = cur_array[i];
    }
}

static struct mod_info numabalance_info[] = {
    {"fault", DETAIL_BIT,  0,  STATS_NULL},
    {"local", DETAIL_BIT,  0,  STATS_NULL},
    {"migrate", DETAIL_BIT,  0,  STATS_NULL}
};

void
mod_register(struct module *mod)
{
    register_mod_fields(mod, "--numabalance", numabalance_storage,
            numabalance_info, 3, read_vmstat_numabalance, set_numabalance_record);
}
