#include "tsar.h"

struct stats_vmevent {
    unsigned long long kswapd_reclaim;          /* pageoutrun */
    unsigned long long pgsteal_kswapd;
    unsigned long long pgsteal_direct;
    unsigned long long kcompactd_reclaim;       /* compact_daemon_wake */
    unsigned long long direct_compact;          /* compact_stall */
    unsigned long long dcompact_success;        /* compact_success */
    unsigned long long oom_kill;
};

#define NR_EVENT 7
#define STATS_PGEVENT_SIZE       (sizeof(struct stats_vmevent))

static char *vmevent_storage = "    --vmevent             mem main event";

/*
 *************************************************************
 * Read vmevent statistics from /proc/vmstat.
 *************************************************************
 */
static void
read_vmstat_vmevent(struct module *mod)
{
    FILE              *fp;
    char               line[LEN_4096], buf[LEN_4096];
    struct stats_vmevent  st_vmevent;

    memset(buf, 0, LEN_4096);
    memset(&st_vmevent, 0, sizeof(struct stats_vmevent));
    /* read /proc/vmstat*/
    if ((fp = fopen(VMSTAT, "r")) == NULL) {
        return ;
    }

    while (fgets(line, LEN_4096, fp) != NULL) {

        if (!strncmp(line, "pageoutrun ", 11)) {
            /* Read number of swap pages brought in */
            sscanf(line + 11, "%llu", &st_vmevent.kswapd_reclaim);

        } else if (!strncmp(line, "pgsteal_kswapd ", 15)) {
            /* Read number of swap pages brought out */
            sscanf(line + 14, "%llu", &st_vmevent.pgsteal_kswapd);

        } else if (!strncmp(line, "pgsteal_direct ", 15)) {
            /* Read number of direct reclaim page scan */ 
            sscanf(line + 14, "%llu", &st_vmevent.pgsteal_direct);

        } else if (!strncmp(line, "compact_daemon_wake ", 20)) {
            /* Read number of kcompactd wake */ 
            sscanf(line + 20, "%llu", &st_vmevent.kcompactd_reclaim);

        } else if (!strncmp(line, "compact_stall ", 14)) {
            /* Read number of direct copmpact */ 
            sscanf(line + 14, "%llu", &st_vmevent.direct_compact);

        } else if (!strncmp(line, "compact_success ", 16)) {
            /* Read number of direct copmpact success */ 
            sscanf(line + 16, "%llu", &st_vmevent.dcompact_success);

        } else if (!strncmp(line, "oom_kill ", 9)) {
            /* Read number of oom kill */ 
            sscanf(line + 9, "%llu", &st_vmevent.oom_kill);
        }
    }
    fclose(fp);
    sprintf(buf, "%lld,%lld,%lld,%lld,%lld,%lld,%lld", st_vmevent.kswapd_reclaim, st_vmevent.pgsteal_kswapd, st_vmevent.pgsteal_direct, 
            st_vmevent.kcompactd_reclaim, st_vmevent.direct_compact, st_vmevent.dcompact_success, st_vmevent.oom_kill);
    set_mod_record(mod, buf);
    return;
}

static void
set_vmevent_record(struct module *mod, double st_array[],
    U_64 pre_array[], U_64 cur_array[], int inter)
{
    int i;
    for (i = 0; i < NR_EVENT; i++) {
        st_array[i] = cur_array[i];
    }
}

static struct mod_info vmevent_info[] = {
    {"kswapd", DETAIL_BIT,  0,  STATS_NULL},
    {"pg_kr", DETAIL_BIT,  0,  STATS_NULL},
    {"pg_dr", DETAIL_BIT,  0,  STATS_NULL},
    {"kcompd", DETAIL_BIT,  0,  STATS_NULL},
    {"dc_all", DETAIL_BIT,  0,  STATS_NULL},
    {"dc_fin", DETAIL_BIT,  0,  STATS_NULL},
    {"  oom", DETAIL_BIT,  0,  STATS_NULL}
};

void
mod_register(struct module *mod)
{
	mod->lable = NULL;
    register_mod_fields(mod, "--vmevent", vmevent_storage, vmevent_info, 7, read_vmstat_vmevent, set_vmevent_record);
}
