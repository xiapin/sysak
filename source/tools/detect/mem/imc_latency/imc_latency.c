#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>
#include <memory.h>
#include <errno.h>
#include <argp.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/types.h>

#include "imc_latency.h"

// #define DEBUG

const char* argp_program_version = "imc_latency 0.1";
const char argp_program_doc[] =
    "Detect the memory latency based on IMC PMU.\n"
    "\n"

    "USAGE: imc_latency [--help] [-d DELAY] [-i ITERATION] [-f LOGFILE]\n"
    "\n"

    "EXAMPLES:\n"
    "    imc_latency            # run forever, display the memory latency.\n"
    "    imc_latency -f foo.log   # log to foo.log.\n";

static const struct argp_option opts[] = {
    {"delay", 'd', "DELAY", 0, "Sample peroid, default is 3 seconds"},
    {"iter", 'i', "ITERATION", 0, "Output times, default run forever"},
    {"logfile", 'f', "LOGFILE", 0,
     "Logfile for result, default /var/log/sysak/imc_latency/imc_latency.log"},
    {NULL, 'h', NULL, OPTION_HIDDEN, "Show the full help"},
    {},
};

struct Env {
    uint32_t max_cpuid;
    int32_t cpu_model;
    int32_t cpu_family;
    int32_t cpu_stepping;
    bool vm;
    int64_t nr_cpu;
    int64_t nr_socket;
    int64_t nr_core;
    int64_t nr_channel;
    int64_t* socket_ref_core;
    int64_t nr_iter;
    int64_t delay;
} env = {.vm = false, .nr_iter = INT64_MAX, .delay = DEFAUlT_PEROID};

record before, after;
time_t before_ts = 0, after_ts = 0;
imc_pmu* pmus = 0;
char log_dir[FILE_PATH_LEN] = "/var/log/sysak/imc_latency";
char default_log_path[FILE_PATH_LEN] =
    "/var/log/sysak/imc_latency/imc_latency.log";
char* log_path = 0;
FILE* log_fp = 0;
bool exiting = false;

static void sigint_handler(int signo) { exiting = 1; }

/* if out of range or no number found return nonzero */
static int parse_long(const char* str, long* retval) {
    int err = 0;
    char* endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);

    /* Check for various possible errors */
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
        (errno != 0 && val == 0)) {
        fprintf(stderr, "Failed parse val.\n");
        err = errno;
        return err;
    }

    if (endptr == str) return err = -1;
    *retval = val;
    return err;
}

static error_t parse_arg(int key, char* arg, struct argp_state* state) {
    int err = 0;
    long val;
    switch (key) {
        case 'h':
            argp_state_help(state, stderr, ARGP_HELP_STD_HELP);
            break;
        case 'd':
            err = parse_long(arg, &val);
            if (err || val <= 0) {
                fprintf(stderr, "Failed parse delay.\n");
                argp_usage(state);
            }

            env.delay = val;
            break;
        case 'i':
            err = parse_long(arg, &val);
            if (err || val <= 0) {
                fprintf(stderr, "Failed parse iteration-num.\n");
                argp_usage(state);
            }
            env.nr_iter = val;
            env.nr_iter++;
            break;
        case 'f':
            log_path = arg;
            break;
        case ARGP_KEY_ARG:
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static int prepare_directory(char* path) {
    int ret;

    ret = mkdir(path, 0777);
    if (ret < 0 && errno != EEXIST)
        return errno;
    else
        return 0;
}

static FILE* open_logfile() {
    FILE* f = 0;
    if (!log_path) {
        log_path = default_log_path;
    }

    f = fopen(log_path, "w");

    return f;
}

int64_t read_sys_file(char* path, bool slient) {
    int64_t val;
    FILE* fp = fopen(path, "r");
    if (!fp) {
        if (!slient) fprintf(stderr, "Failed open sys-file: %s\n", path);
        return -1;
    }

    fscanf(fp, "%ld\n", &val);
#ifdef DEBUG
    fprintf(stderr, "read from=%s val=%ld\n", path, val);
#endif
    if (fp) fclose(fp);
    return val;
}

static int write_reg(imc_event* ev, uint64_t val) {
    int err = 0;
    if (ev->fd >= 0) {
        close(ev->fd);
        ev->fd = -1;
    }

    ev->attr.config = ev->fixed ? 0xff : val;

    if ((ev->fd = syscall(SYS_perf_event_open, &ev->attr, -1, ev->core_id, -1,
                          0)) <= 0) {
        fprintf(stderr, "Linux Perf: Error on programming PMU %d:%s\n",
                ev->pmu_id, strerror(errno));
        fprintf(stderr, "config: 0x%llx config1: 0x%llx config2: 0x%llx\n",
                ev->attr.config, ev->attr.config1, ev->attr.config2);
        if (errno == EMFILE) fprintf(stderr, "%s", ULIMIT_RECOMMENDATION);

        return -1;
    }
    return err;
}

static uint64_t read_reg(imc_event* ev) {
    uint64_t result = 0;
    if (ev->fd >= 0) {
        int status = read(ev->fd, &result, sizeof(result));
        if (status != sizeof(result)) {
            fprintf(
                stderr,
                "PCM Error: failed to read from Linux perf handle %d PMU %d\n",
                ev->fd, ev->pmu_id);
        }
    }
    return result;
}

static bool is_cpu_online(int cpu_id) {
    char path[BUF_SIZE];
    uint64_t val;
    bool res = false;

    snprintf(path, BUF_SIZE, "/sys/devices/system/cpu/cpu%d/online", cpu_id);

    FILE* fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Failed open %s.\n", path);
        goto cleanup;
    }

    val = read_sys_file(path, true);
    if (val == UINT64_MAX) {
        goto cleanup;
    }

    res = true;

cleanup:
    if (fp) fclose(fp);
    return res;
}

int64_t read_core_id(int cpu_id) {
    char core_id_path[BUF_SIZE];
    int64_t val = -1;

    snprintf(core_id_path, BUF_SIZE,
             "/sys/devices/system/cpu/cpu%d/topology/core_id", cpu_id);
    val = read_sys_file(core_id_path, true);
    return val;
}

int64_t read_physical_package_id(int cpu_id) {
    char pkg_id_path[BUF_SIZE];

    int64_t val = -1;

    snprintf(pkg_id_path, BUF_SIZE,
             "/sys/devices/system/cpu/cpu%d/topology/physical_package_id",
             cpu_id);
    val = read_sys_file(pkg_id_path, true);

    return val;
}

static int get_topology(int id, struct topology_ent* ent) {
    int err = 0;
    ent->core_id = read_core_id(id);
    ent->socket_id = read_physical_package_id(id);
    if (ent->core_id == -1 || ent->socket_id == -1) {
#ifdef DEBUG
        fprintf(stderr, "get coreid=%d socket_id=%d\n", ent->core_id,
                ent->socket_id);
#endif
        err = -1;
    }

    return err;
}

static int discovery_topology() {
    int err = 0, i = 0;
    struct topology_ent* topo = 0;

    env.nr_cpu = sysconf(_SC_NPROCESSORS_CONF);

    if (env.nr_cpu < 0) {
        fprintf(stderr, "Failed get nr_cpu.\n");
        err = -1;
        goto cleanup;
    }

    topo = calloc(env.nr_cpu, sizeof(struct topology_ent));
    if (!topo) {
        fprintf(stderr, "Faile calloc topology memory.\n");
        err = -1;
        goto cleanup;
    }

    int64_t max_skt_id = 0;
    int64_t max_core_id = 0;
    for (i = 0; i < env.nr_cpu; i++) {
        err = get_topology(i, topo + i);
        if (err) {
            fprintf(stderr, "Failed get topology cpuid:%d\n", i);
            goto cleanup;
        }

        max_skt_id =
            max_skt_id > topo[i].socket_id ? max_skt_id : topo[i].socket_id;
        max_core_id =
            max_core_id > topo[i].core_id ? max_core_id : topo[i].core_id;
    }

    env.nr_socket = max_skt_id + 1;
    env.nr_core = max_core_id + 1;

    env.socket_ref_core = calloc(env.nr_socket, sizeof(int64_t));
    if (!env.socket_ref_core) {
        fprintf(stderr, "Failed calloc socket_ref_core. nr_socket=%d\n",
                env.nr_socket);
        err = -1;
        goto cleanup;
    }

    for (i = 0; i < env.nr_cpu; i++) {
        if (!is_cpu_online(i)) continue;
        env.socket_ref_core[topo[i].socket_id] = i;
    }

cleanup:
    if (topo) free(topo);
    topo = 0;
    return err;
}

static void cpuid_1(int leaf, CPUID_INFO* info) {
    __asm__ __volatile__("cpuid"
                         : "=a"(info->reg.eax), "=b"(info->reg.ebx),
                           "=c"(info->reg.ecx), "=d"(info->reg.edx)
                         : "a"(leaf));
}

void cpuid_2(const unsigned leaf, const unsigned subleaf, CPUID_INFO* info) {
    __asm__ __volatile__("cpuid"
                         : "=a"(info->reg.eax), "=b"(info->reg.ebx),
                           "=c"(info->reg.ecx), "=d"(info->reg.edx)
                         : "a"(leaf), "c"(subleaf));
}

static bool detect_model() {
    char buffer[1024];
    union {
        char cbuf[16];
        int ibuf[16 / sizeof(int)];
    } buf;

    CPUID_INFO cpuinfo;

    bzero(buffer, 1024);
    bzero(buf.cbuf, 16);
    cpuid_1(0, &cpuinfo);

    buf.ibuf[0] = cpuinfo.array[1];
    buf.ibuf[1] = cpuinfo.array[3];
    buf.ibuf[2] = cpuinfo.array[2];

    if (strncmp(buf.cbuf, "GenuineIntel", 4 * 3) != 0) {
        fprintf(stderr, "Not intel cpu.\n");
        return false;
    }

    env.max_cpuid = cpuinfo.array[0];

    cpuid_1(1, &cpuinfo);
    env.cpu_family = (((cpuinfo.array[0]) >> 8) & 0xf) |
                     ((cpuinfo.array[0] & 0xf00000) >> 16);
    env.cpu_model = (((cpuinfo.array[0]) & 0xf0) >> 4) |
                    ((cpuinfo.array[0] & 0xf0000) >> 12);
    env.cpu_stepping = cpuinfo.array[0] & 0x0f;

    if (cpuinfo.reg.ecx & (1UL << 31UL)) {
        env.vm = true;
        fprintf(stderr,
                "WARN: Detected a hypervisor/virtualization technology. Some "
                "metrics might not be available due to configuration or "
                "availability of virtual hardware features.\n");
    }

    if (env.cpu_family != 6) {
        fprintf(stderr, "Unsupport CPU Family: %d\n", env.cpu_family);
        return false;
    }

    return true;
}

bool is_model_support() {
    switch (env.cpu_model) {
        case NEHALEM:
            env.cpu_model = NEHALEM_EP;
            break;
        case ATOM_2:
            env.cpu_model = ATOM;
            break;
        case HASWELL_ULT:
        case HASWELL_2:
            env.cpu_model = HASWELL;
            break;
        case BROADWELL_XEON_E3:
            env.cpu_model = BROADWELL;
            break;
        case ICX_D:
            env.cpu_model = ICX;
            break;
        case CML_1:
            env.cpu_model = CML;
            break;
        case ICL_1:
            env.cpu_model = ICL;
            break;
        case TGL_1:
            env.cpu_model = TGL;
            break;
        case ADL_1:
            env.cpu_model = ADL;
            break;
        case RPL_1:
        case RPL_2:
        case RPL_3:
            env.cpu_model = RPL;
            break;
    }

    return (env.cpu_model == ICX || env.cpu_model == SPR ||
            env.cpu_model == SKX);
}

uint32_t* get_ddr_latency_metric_config() {
    uint32_t* cfgs = 0;
    cfgs = calloc(4, sizeof(uint32_t));
    if (!cfgs) {
        fprintf(stderr, "Failed calloc cfgs memory.\n");
        return NULL;
    }

    if (ICX == env.cpu_model || SPR == env.cpu_model) {
        cfgs[0] = MC_CH_PCI_PMON_CTL_EVENT(0x80) +
                  MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM RPQ occupancy pch 0
        cfgs[1] = MC_CH_PCI_PMON_CTL_EVENT(0x10) +
                  MC_CH_PCI_PMON_CTL_UMASK(1);  // DRAM RPQ Insert.pch 0
        cfgs[2] = MC_CH_PCI_PMON_CTL_EVENT(0x82) +
                  MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM WPQ Occupancy pch 0
        cfgs[3] = MC_CH_PCI_PMON_CTL_EVENT(0x20) +
                  MC_CH_PCI_PMON_CTL_UMASK(1);  // DRAM WPQ Insert.pch 0
    } else {
        cfgs[0] = MC_CH_PCI_PMON_CTL_EVENT(0x80) +
                  MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM RPQ occupancy
        cfgs[1] = MC_CH_PCI_PMON_CTL_EVENT(0x10) +
                  MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM RPQ Insert
        cfgs[2] = MC_CH_PCI_PMON_CTL_EVENT(0x81) +
                  MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM WPQ Occupancy
        cfgs[3] = MC_CH_PCI_PMON_CTL_EVENT(0x20) +
                  MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM WPQ Insert
    }

    return cfgs;
}

struct perf_event_attr init_perf_event_attr(bool group) {
    struct perf_event_attr e;
    bzero(&e, sizeof(struct perf_event_attr));
    e.type = -1;  // must be set up later
    e.size = sizeof(e);
    e.config = -1;  // must be set up later
    e.read_format = group ? PERF_FORMAT_GROUP
                          : 0; /* PERF_FORMAT_TOTAL_TIME_ENABLED |
      PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID | PERF_FORMAT_GROUP ; */
    return e;
}

void init_imc_event(imc_event* event, int pmu_id, int core_id, bool fixed) {
    struct perf_event_attr attr = init_perf_event_attr(false);
    attr.type = pmu_id;
    event->attr = attr;
    event->fixed = fixed;
    event->pmu_id = pmu_id;
    event->core_id = core_id;
    event->fd = -1;
}

void init_imc_reggrp(imc_reg_group* grp, int socket_id, int pmu_id) {
    int i = 0;
#ifdef DEBUG

    fprintf(stderr, "Init imc reg group: socketid=%d pmuid=%d\n", socket_id,
            pmu_id);
#endif
    init_imc_event(&grp->fixed_ev, pmu_id, env.socket_ref_core[socket_id],
                   true);

    for (i = 0; i < GENERAL_REG_NUM; i++) {
        init_imc_event(&grp->general_ev[i], pmu_id,
                       env.socket_ref_core[socket_id], false);
    }
}

imc_pmu* init_imc_pmus(int64_t* pmu_ids, int64_t size) {
    int skt_id = 0;
    int pmu_id = 0;

    imc_pmu* pmus = calloc(env.nr_socket, sizeof(imc_pmu));

    for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
        pmus[skt_id].reg_groups = calloc(size, sizeof(imc_reg_group));
        pmus[skt_id].socket_id = skt_id;
        pmus[skt_id].nr_grp = size;

        for (pmu_id = 0; pmu_id < size; pmu_id++) {
            init_imc_reggrp(&pmus[skt_id].reg_groups[pmu_id], skt_id,
                            pmu_ids[pmu_id]);
        }
    }

    return pmus;
}

void program_imc(uint32_t* cfgs, imc_pmu* pmus) {
    int skt_id = 0;
    int pmu_id = 0;
    int idx = 0;
    for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
        imc_pmu* pmu = pmus + skt_id;
        for (pmu_id = 0; pmu_id < pmu->nr_grp; pmu_id++) {
            imc_reg_group* grp = pmu->reg_groups + pmu_id;
            /* enabel and reset fixed counter(DRAM clock) */
            write_reg(&grp->fixed_ev, MC_CH_PCI_PMON_FIXED_CTL_EN);
            write_reg(&grp->fixed_ev, MC_CH_PCI_PMON_FIXED_CTL_EN +
                                          MC_CH_PCI_PMON_FIXED_CTL_RST);
            for (idx = 0; idx < GENERAL_REG_NUM; idx++) {
                uint64_t event = cfgs[idx];
                if (SPR == env.cpu_model) {
                    write_reg(&grp->general_ev[idx], event);
                } else {
                    write_reg(&grp->general_ev[idx], MC_CH_PCI_PMON_CTL_EN);
                    write_reg(&grp->general_ev[idx],
                              MC_CH_PCI_PMON_CTL_EN | event);
                }
            }
        }
    }
}

socket_record* alloc_socket_record() {
    int skt_id = 0;
    socket_record* rec = calloc(env.nr_socket, sizeof(socket_record));
    for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
        rec[skt_id].channel_record_arr =
            calloc(env.nr_channel, sizeof(channel_record));
    }
    return rec;
}

void free_socket_record(socket_record* rec) {
    int skt_id = 0;
    for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
        free(rec[skt_id].channel_record_arr);
    }
    free(rec);
}

void init_data() {
    before.socket_record_arr = alloc_socket_record();
    after.socket_record_arr = alloc_socket_record();
}

void free_data() {
    free_socket_record(before.socket_record_arr);
    free_socket_record(after.socket_record_arr);
}

int64_t get_perf_pmuid(int num) {
    int64_t id = -1;
    char imc_path[BUF_SIZE];

    if (num != -1) {
        snprintf(imc_path, BUF_SIZE,
                 "/sys/bus/event_source/devices/uncore_imc_%d/type", num);
    } else {
        snprintf(imc_path, BUF_SIZE,
                 "/sys/bus/event_source/devices/uncore_imc/type");
    }

    id = read_sys_file(imc_path, true);

    return id;
}

static int64_t* enumerate_imc_PMUs() {
    int64_t* pmu_ids = 0;
    int idx = 0, i = 0;

    pmu_ids = calloc(MAX_IMC_ID, sizeof(int64_t));

    if (!pmu_ids) {
        fprintf(stderr, "Failed calloc pmu ids memory.\n");
        return NULL;
    }

    for (i = -1; i <= MAX_IMC_ID; ++i) {
        int64_t pmu_id = get_perf_pmuid(i);
        if (pmu_id != -1) pmu_ids[idx++] = pmu_id;
    }

    env.nr_channel = idx;

cleanup:

    if (env.nr_channel == 0 && pmu_ids) {
        free(pmu_ids);
        pmu_ids = 0;
    }

    return pmu_ids;
}

static int init_env() {
    int err = 0;
    int64_t* pmu_ids = 0;
    uint32_t* cfgs = 0;

    // check model
    if (!detect_model()) {
        fprintf(stderr, "Failed detect model.\n");
        err = -1;
        goto cleanup;
    }

    if (!is_model_support()) {
        fprintf(stderr, "Unsupport model.\n");
        err = -1;
        goto cleanup;
    }

    // get core/socket info
    err = discovery_topology();
    if (err) {
        fprintf(stderr, "Failed discovery topology.\n");
        err = -1;
        goto cleanup;
    }

    // get all imc-pmu id
    pmu_ids = enumerate_imc_PMUs();
    if (!pmu_ids) {
        fprintf(stderr, "Failed enumerate imc pmus.\n");
        err = -1;
        goto cleanup;
    }

    cfgs = get_ddr_latency_metric_config();
    if (!cfgs) {
        fprintf(stderr, "Failed enumerate imc pmus.\n");
        err = -1;
        goto cleanup;
    }

    // init pmu
    pmus = init_imc_pmus(pmu_ids, env.nr_channel);

    // write pmu register
    program_imc(cfgs, pmus);

    // init data
    init_data();
#ifdef DEBUG
    fprintf(stderr, "nr_socket=%d nr_core=%d nr_cpu=%d nr_channel=%d \n",
            env.nr_socket, env.nr_core, env.nr_cpu, env.nr_channel);
    int i = 0;
    for (i = 0; i < env.nr_socket; i++) {
        fprintf(stderr, "socket%d-ref cpu=%d\n", i, env.socket_ref_core[i]);
    }
#endif

cleanup:

    if (pmu_ids) {
        free(pmu_ids);
        pmu_ids = 0;
    }

    if (cfgs) {
        free(cfgs);
        cfgs = 0;
    }

    return err;
}

void read_imc() {
    int skt_id = 0, pmu_id = 0, counter_id = 0;
    after_ts = time(0);

    for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
        imc_pmu* pmu = pmus + skt_id;
        socket_record* socket_ev = &after.socket_record_arr[skt_id];
        for (pmu_id = 0; pmu_id < pmu->nr_grp; pmu_id++) {
            imc_reg_group* grp = pmu->reg_groups + pmu_id;
            channel_record* channel_ev =
                &after.socket_record_arr[skt_id].channel_record_arr[pmu_id];
            /* enabel and reset fixed counter(DRAM clock) */
            if (pmu_id == 0) {
                socket_ev->dram_clock = read_reg(&grp->fixed_ev);
                if (env.cpu_model == ICX || env.cpu_model == SNOWRIDGE) {
                    socket_ev->dram_clock = 2 * socket_ev->dram_clock;
                }
            }

            channel_ev->rpq_occ = read_reg(&grp->general_ev[RPQ_OCC]);
            channel_ev->rpq_ins = read_reg(&grp->general_ev[RPQ_INS]);
            channel_ev->wpq_occ = read_reg(&grp->general_ev[WPQ_OCC]);
            channel_ev->wpq_ins = read_reg(&grp->general_ev[WPQ_INS]);

            socket_ev->rpq_occ += channel_ev->rpq_occ;
            socket_ev->rpq_ins += channel_ev->rpq_ins;
            socket_ev->wpq_occ += channel_ev->wpq_occ;
            socket_ev->wpq_ins += channel_ev->wpq_ins;
        }
    }

    if (before_ts) {
        double delta = after_ts - before_ts;
        for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
            socket_record* before_socket_ev = &before.socket_record_arr[skt_id];
            socket_record* after_socket_ev = &after.socket_record_arr[skt_id];
            imc_pmu* pmu = pmus + skt_id;
            double dram_speed =
                (after_socket_ev->dram_clock - before_socket_ev->dram_clock) /
                (delta * (double)1e9);

            for (pmu_id = 0; pmu_id < pmu->nr_grp; pmu_id++) {
                channel_record* before_channel_ev =
                    &before_socket_ev->channel_record_arr[pmu_id];
                channel_record* after_channel_ev =
                    &after_socket_ev->channel_record_arr[pmu_id];

                if (after_channel_ev->rpq_ins - before_channel_ev->rpq_ins >
                    0) {
                    after_channel_ev->read_latency =
                        (after_channel_ev->rpq_occ -
                         before_channel_ev->rpq_occ) /
                        (after_channel_ev->rpq_ins -
                         before_channel_ev->rpq_ins) /
                        dram_speed;
                }

                if (after_channel_ev->wpq_ins - before_channel_ev->wpq_ins >
                    0) {
                    after_channel_ev->write_latency =
                        (after_channel_ev->wpq_occ -
                         before_channel_ev->wpq_occ) /
                        (after_channel_ev->wpq_ins -
                         before_channel_ev->wpq_ins) /
                        dram_speed;
                }
            }

            if (after_socket_ev->rpq_ins - before_socket_ev->rpq_ins > 0) {
                after_socket_ev->read_latency =
                    (after_socket_ev->rpq_occ - before_socket_ev->rpq_occ) /
                    (after_socket_ev->rpq_ins - before_socket_ev->rpq_ins) /
                    dram_speed;
            }

            if (after_socket_ev->wpq_ins - before_socket_ev->wpq_ins > 0) {
                after_socket_ev->write_latency =
                    (after_socket_ev->wpq_occ - before_socket_ev->wpq_occ) /
                    (after_socket_ev->wpq_ins - before_socket_ev->wpq_ins) /
                    dram_speed;
            }
        }
    }
}

static char* ts2str(time_t ts, char* buf, int size) {
    struct tm* t = gmtime(&ts);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
    return buf;
}

static void output_ts(FILE* dest) {
    char stime_str[BUF_SIZE] = {0};
    time_t now = time(0);
    fprintf(dest, "[TIME-STAMP] %s\n", ts2str(now, stime_str, BUF_SIZE));
}

static void output_socket_lat(FILE* dest) {
    int32_t socket_id = 0;

    fprintf(dest, "%s\n", "[SOCKET_LEVEL]");
    // fprintf(dest, "%8s%16s%16s\n", "socket", "rlat", "wlat");
    fprintf(dest, "%8s", "");

    for (socket_id = 0; socket_id < env.nr_socket; socket_id++) {
        fprintf(dest, "%8d", socket_id);
    }
    fprintf(dest, "\n");

    fprintf(dest, "%8s", "rlat");
    for (socket_id = 0; socket_id < env.nr_socket; socket_id++) {
        socket_record* srec = &after.socket_record_arr[socket_id];
        fprintf(dest, "%8.2lf", srec->read_latency);
    }
    fprintf(dest, "\n");

    fprintf(dest, "%8s", "wlat");
    for (socket_id = 0; socket_id < env.nr_socket; socket_id++) {
        socket_record* srec = &after.socket_record_arr[socket_id];
        fprintf(dest, "%8.2lf", srec->write_latency);
    }
    fprintf(dest, "\n");
}

static void output_channel_lat(FILE* dest) {
    int32_t socket_id = 0, channel_id = 0;
    for (socket_id = 0; socket_id < env.nr_socket; socket_id++) {
        char socket_name[32];
        snprintf(socket_name, 32, "%d", socket_id);

        socket_record* srec = &after.socket_record_arr[socket_id];

        fprintf(dest, "[CHANNEL_LEVEL]-[SOCKET-%d]\n", socket_id);
        fprintf(dest, "%8s", "");
        for (channel_id = 0; channel_id < env.nr_channel; channel_id++) {
            fprintf(dest, "%8d", channel_id);
        }
        fprintf(dest, "\n");

        fprintf(dest, "%8s", "rlat");
        for (channel_id = 0; channel_id < env.nr_channel; channel_id++) {
            channel_record* crec = &srec->channel_record_arr[channel_id];
            fprintf(dest, "%8.2lf", crec->read_latency);
        }
        fprintf(dest, "\n");

        fprintf(dest, "%8s", "wlat");
        for (channel_id = 0; channel_id < env.nr_channel; channel_id++) {
            channel_record* crec = &srec->channel_record_arr[channel_id];
            fprintf(dest, "%8.2lf", crec->write_latency);
        }
        fprintf(dest, "\n");
    }
}

void swap_record() {
    /* swap data */
    socket_record* tmp = before.socket_record_arr;
    before.socket_record_arr = after.socket_record_arr;
    after.socket_record_arr = tmp;

    /* clear after data */
    free_socket_record(after.socket_record_arr);
    after.socket_record_arr = alloc_socket_record();

    /* reset before timestamp */
    before_ts = after_ts;
}

static void output_split(FILE* dest) { fprintf(dest, "\n"); }
static void collect_data() {
    int32_t socket_id = 0, channel_id = 0, line_num = 0;
    read_imc();

    if (before_ts) {
        output_ts(log_fp);
        output_socket_lat(log_fp);
        output_channel_lat(log_fp);
        output_split(log_fp);
        fflush(log_fp);
    }

    swap_record();
}

static void clean_env(void) { free_data(); }

int main(int argc, char** argv) {
    int err;
    /* parse args */
    static const struct argp argp = {
        .options = opts,
        .parser = parse_arg,
        .doc = argp_program_doc,
    };

    err = argp_parse(&argp, argc, argv, 0, 0, 0);
    if (err) {
        fprintf(stderr, "Failed parse args.\n");
        return -1;
    }

    prepare_directory(log_dir);
    log_fp = open_logfile();
    if (!log_fp) {
        fprintf(stderr, "Failed open log file.\n");
        return -1;
    }

    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        fprintf(stderr, "Failed set signal handler.\n");
        return -errno;
    }

    err = init_env();
    if (err) {
        fprintf(stderr, "Init env error.\n");
        return -1;
    }
    
    while (env.nr_iter-- && !exiting) {
        collect_data();
        sleep(env.delay);
    }

    clean_env();
}