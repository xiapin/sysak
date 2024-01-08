#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <memory.h>
#include <errno.h>
#include <strings.h>
#include <sys/syscall.h>
#include <linux/types.h>
#include <sys/time.h>

#include "uncore_imc.h"

#define NR_IMC 6

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
    int64_t *socket_ref_core;
    bool init_succ;
} env = {.vm = false, .init_succ = true};

// #define DEBUG
#ifdef DEBUG
void print_metric(metric *m) {
    printf("rlat=%lf wlat=%lf avglat=%lf bw_rd=%ld bw_wr=%ld\n", m->rlat,
           m->wlat, m->avglat, m->bw_rd, m->bw_wr);
}

void print_result(result res) {
    int i, j;
    for (int i = 0; i < env.nr_socket; i++) {
        // for (int j = 0; j < env.nr_channel; j++) {
        //     printf("socket=%d channel=%d\n", i, j);
        //     print_metric(&res.channel[i][j]);
        // }
        printf("socket=%d\n", i);
        print_metric(&res.socket[i]);
    }
    printf("node:\n");
    print_metric(res.node);
}
#endif

record before, after;
result res;

time_t before_ts = 0, after_ts = 0;
imc_pmu *pmus = 0;

int64_t read_sys_file(char *path, bool slient) {
    int64_t val;
    FILE *fp = fopen(path, "r");
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

static int write_reg(imc_event *ev, uint64_t val) {
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

static uint64_t read_reg(imc_event *ev) {
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

    FILE *fp = fopen(path, "r");
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

static int get_topology(int id, struct topology_ent *ent) {
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
    struct topology_ent *topo = 0;

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

static void cpuid_1(int leaf, CPUID_INFO *info) {
    __asm__ __volatile__("cpuid"
                         : "=a"(info->reg.eax), "=b"(info->reg.ebx),
                           "=c"(info->reg.ecx), "=d"(info->reg.edx)
                         : "a"(leaf));
}

void cpuid_2(const unsigned leaf, const unsigned subleaf, CPUID_INFO *info) {
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

uint32_t *get_ddr_latency_metric_config() {
    uint32_t *cfgs = 0;
    cfgs = calloc(4, sizeof(uint32_t));
    if (!cfgs) {
        fprintf(stderr, "Failed calloc cfgs memory.\n");
        return NULL;
    }

    if (ICX == env.cpu_model || SPR == env.cpu_model) {
        cfgs[RPQ_OCC] =
            MC_CH_PCI_PMON_CTL_EVENT(0x80) +
            MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM RPQ occupancy pch 0
        cfgs[RPQ_INS] = MC_CH_PCI_PMON_CTL_EVENT(0x10) +
                        MC_CH_PCI_PMON_CTL_UMASK(1);  // DRAM RPQ Insert.pch 0
        cfgs[WPQ_OCC] =
            MC_CH_PCI_PMON_CTL_EVENT(0x82) +
            MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM WPQ Occupancy pch 0
        cfgs[WPQ_INS] = MC_CH_PCI_PMON_CTL_EVENT(0x20) +
                        MC_CH_PCI_PMON_CTL_UMASK(1);  // DRAM WPQ Insert.pch 0
    } else {
        cfgs[RPQ_OCC] = MC_CH_PCI_PMON_CTL_EVENT(0x80) +
                        MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM RPQ occupancy
        cfgs[RPQ_INS] = MC_CH_PCI_PMON_CTL_EVENT(0x10) +
                        MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM RPQ Insert
        cfgs[WPQ_OCC] = MC_CH_PCI_PMON_CTL_EVENT(0x81) +
                        MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM WPQ Occupancy
        cfgs[WPQ_INS] = MC_CH_PCI_PMON_CTL_EVENT(0x20) +
                        MC_CH_PCI_PMON_CTL_UMASK(0);  // DRAM WPQ Insert
    }

    /* CAS_COUNT.RD and CAS_COUNT.WR */
    switch (env.cpu_model) {
        case KNL:
            cfgs[CAS_RD] =
                MC_CH_PCI_PMON_CTL_EVENT(0x03) + MC_CH_PCI_PMON_CTL_UMASK(1);
            cfgs[CAS_WR] =
                MC_CH_PCI_PMON_CTL_EVENT(0x03) + MC_CH_PCI_PMON_CTL_UMASK(2);
        case SNOWRIDGE:
        case ICX:
            cfgs[CAS_RD] =
                MC_CH_PCI_PMON_CTL_EVENT(0x04) + MC_CH_PCI_PMON_CTL_UMASK(0x0f);
            cfgs[CAS_WR] =
                MC_CH_PCI_PMON_CTL_EVENT(0x04) + MC_CH_PCI_PMON_CTL_UMASK(0x30);
        case SPR:
            cfgs[CAS_RD] =
                MC_CH_PCI_PMON_CTL_EVENT(0x05) + MC_CH_PCI_PMON_CTL_UMASK(0xcf);
            cfgs[CAS_WR] =
                MC_CH_PCI_PMON_CTL_EVENT(0x05) + MC_CH_PCI_PMON_CTL_UMASK(0xf0);
        default:
            cfgs[CAS_RD] =
                MC_CH_PCI_PMON_CTL_EVENT(0x04) + MC_CH_PCI_PMON_CTL_UMASK(3);
            cfgs[CAS_WR] =
                MC_CH_PCI_PMON_CTL_EVENT(0x04) + MC_CH_PCI_PMON_CTL_UMASK(12);
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

void init_imc_event(imc_event *event, int pmu_id, int core_id, bool fixed) {
    struct perf_event_attr attr = init_perf_event_attr(false);

    attr.type = pmu_id;
    event->attr = attr;
    event->fixed = fixed;
    event->pmu_id = pmu_id;
    event->core_id = core_id;
    event->fd = -1;
}

void init_imc_reggrp(imc_reg_group *grp, int socket_id, int pmu_id) {
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

imc_pmu *init_imc_pmus(int64_t *pmu_ids, int64_t size) {
    int skt_id = 0;
    int pmu_id = 0;

    imc_pmu *pmus = calloc(env.nr_socket, sizeof(imc_pmu));

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

void program_imc(uint32_t *cfgs, imc_pmu *pmus) {
    int skt_id = 0;
    int pmu_id = 0;
    int idx = 0;
    for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
        imc_pmu *pmu = pmus + skt_id;
        for (pmu_id = 0; pmu_id < pmu->nr_grp; pmu_id++) {
            imc_reg_group *grp = pmu->reg_groups + pmu_id;
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

reg_event **alloc_record() {
    int i = 0;
    reg_event **ret = calloc(env.nr_socket, sizeof(reg_event *));
    for (i = 0; i < env.nr_socket; i++) {
        ret[i] = calloc(env.nr_channel, sizeof(reg_event));
    }
    return ret;
}

void free_record(reg_event **data) {
    int i = 0;
    for (i = 0; i < env.nr_socket; i++) {
        free(data[i]);
    }
    free(data);
}

void alloc_result() {
    int i = 0;
    res.channel = calloc(env.nr_socket, sizeof(metric *));
    for (i = 0; i < env.nr_socket; i++) {
        res.channel[i] = calloc(env.nr_channel, sizeof(metric));
    }

    res.socket = calloc(env.nr_socket, sizeof(metric));
    res.node = calloc(1, sizeof(metric));
}

void free_result() {
    int i = 0;
    for (i = 0; i < env.nr_socket; i++) {
        free(res.channel[i]);
    }
    free(res.channel);

    if (res.socket) free(res.socket);
    if (res.node) free(res.node);
}

void init_data() {
    before.regs = alloc_record();
    after.regs = alloc_record();
    alloc_result();
}

void free_data() {
    free_record(before.regs);
    free_record(after.regs);
    free_result();
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

static int64_t *enumerate_imc_PMUs() {
    int64_t *pmu_ids = 0;
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

int init(void *arg) {
    int err = 0;
    int64_t *pmu_ids = 0;
    uint32_t *cfgs = 0;

    bzero(&before, sizeof(before));
    bzero(&after, sizeof(after));
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

    if (err) {
        env.init_succ = false;
    }
    return err;
}

void read_imc() {
    int skt_id = 0, pmu_id = 0;

    for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
        imc_pmu *pmu = pmus + skt_id;
        for (pmu_id = 0; pmu_id < pmu->nr_grp; pmu_id++) {
            imc_reg_group *grp = pmu->reg_groups + pmu_id;
            reg_event *reg_ev = after.regs[skt_id] + pmu_id;
            /* enabel and reset fixed counter(DRAM clock) */
            reg_ev->dram_clock = read_reg(&grp->fixed_ev);
            if (env.cpu_model == ICX || env.cpu_model == SNOWRIDGE) {
                reg_ev->dram_clock = 2 * reg_ev->dram_clock;
            }

            reg_ev->rpq_occ = read_reg(&grp->general_ev[RPQ_OCC]);
            reg_ev->rpq_ins = read_reg(&grp->general_ev[RPQ_INS]);
            reg_ev->wpq_occ = read_reg(&grp->general_ev[WPQ_OCC]);
            reg_ev->wpq_ins = read_reg(&grp->general_ev[WPQ_INS]);
            reg_ev->cas_rd = read_reg(&grp->general_ev[CAS_RD]);
            reg_ev->cas_wr = read_reg(&grp->general_ev[CAS_WR]);
        }
    }
}

void calculate_metric() {
    int skt_id = 0, pmu_id = 0;
    after_ts = time(0);
    if (before_ts) {
#define UINT48_MAX 281474976710655U /* (1 << 48) - 1 */
#define LAT(dest, occ, ins, speed)          \
    ({                                      \
        if ((ins != 0) && (speed != 0))     \
            dest = (occ) / (ins) / (speed); \
        else                                \
            dest = 0;                       \
    })

#define DELTA(val1, val2) \
    (val1) >= (val2) ? (val1) - (val2) : UINT48_MAX - (val2) + (val1);

        double delta = after_ts - before_ts;
        double dram_speed;
        reg_event node_reg_ev;
        bzero(&node_reg_ev, sizeof(reg_event));

        for (skt_id = 0; skt_id < env.nr_socket; skt_id++) {
            imc_pmu *pmu = pmus + skt_id;
            reg_event skt_reg_ev;
            bzero(&skt_reg_ev, sizeof(skt_reg_ev));
            for (pmu_id = 0; pmu_id < pmu->nr_grp; pmu_id++) {
                reg_event *before_reg_ev = before.regs[skt_id] + pmu_id;
                reg_event *after_reg_ev = after.regs[skt_id] + pmu_id;
                if (pmu_id == 0) {
                    uint64_t clock = DELTA(after_reg_ev->dram_clock,
                                           before_reg_ev->dram_clock);
                    dram_speed = (clock) / (delta * (double)1e9);
                }
                // calculate the channel delta value

                uint64_t delta_rpqocc =
                    DELTA(after_reg_ev->rpq_occ, before_reg_ev->rpq_occ);
                uint64_t delta_rpqins =
                    DELTA(after_reg_ev->rpq_ins, before_reg_ev->rpq_ins);
                uint64_t delta_wpqocc =
                    DELTA(after_reg_ev->wpq_occ, before_reg_ev->wpq_occ);
                uint64_t delta_wpqins =
                    DELTA(after_reg_ev->wpq_ins, before_reg_ev->wpq_ins);
                uint64_t delta_wr =
                    DELTA(after_reg_ev->cas_wr, before_reg_ev->cas_wr);
                uint64_t delta_rd =
                    DELTA(after_reg_ev->cas_rd, before_reg_ev->cas_rd);

                // calculate the channel metric
                res.channel[skt_id][pmu_id].bw_wr = delta_wr * 64;
                res.channel[skt_id][pmu_id].bw_rd = delta_rd * 64;

                LAT(res.channel[skt_id][pmu_id].rlat, delta_rpqocc,
                    delta_rpqins, dram_speed);
                LAT(res.channel[skt_id][pmu_id].wlat, delta_wpqocc,
                    delta_wpqins, dram_speed);
                LAT(res.channel[skt_id][pmu_id].avglat,
                    delta_rpqocc + delta_wpqocc, delta_wpqins + delta_rpqins,
                    dram_speed);

                // accumulate the socket delta value
                skt_reg_ev.rpq_occ += delta_rpqocc;
                skt_reg_ev.rpq_ins += delta_rpqins;
                skt_reg_ev.wpq_occ += delta_wpqocc;
                skt_reg_ev.wpq_ins += delta_wpqins;
                skt_reg_ev.cas_wr += delta_wr;
                skt_reg_ev.cas_rd += delta_rd;
            }

            // calculate the socket metric
            LAT(res.socket[skt_id].rlat, skt_reg_ev.rpq_occ, skt_reg_ev.rpq_ins,
                dram_speed);
            LAT(res.socket[skt_id].wlat, skt_reg_ev.wpq_occ, skt_reg_ev.wpq_ins,
                dram_speed);
            LAT(res.socket[skt_id].avglat,
                skt_reg_ev.wpq_occ + skt_reg_ev.rpq_occ,
                skt_reg_ev.wpq_ins + skt_reg_ev.rpq_ins, dram_speed);
            res.socket[skt_id].bw_rd = skt_reg_ev.cas_rd * 64;
            res.socket[skt_id].bw_wr = skt_reg_ev.cas_wr * 64;
            // accumulate the node delta value
            node_reg_ev.rpq_occ += skt_reg_ev.rpq_occ;
            node_reg_ev.rpq_ins += skt_reg_ev.rpq_ins;
            node_reg_ev.wpq_occ += skt_reg_ev.wpq_occ;
            node_reg_ev.wpq_ins += skt_reg_ev.wpq_ins;
            node_reg_ev.cas_wr += skt_reg_ev.cas_wr;
            node_reg_ev.cas_rd += skt_reg_ev.cas_rd;
        }
        // calculate the node metric
        LAT(res.node->rlat, node_reg_ev.rpq_occ, node_reg_ev.rpq_ins,
            dram_speed);
        LAT(res.node->wlat, node_reg_ev.wpq_occ, node_reg_ev.wpq_ins,
            dram_speed);
        LAT(res.node->avglat, node_reg_ev.wpq_occ + node_reg_ev.rpq_occ,
            node_reg_ev.wpq_ins + node_reg_ev.rpq_ins, dram_speed);
        res.node->bw_rd = node_reg_ev.cas_rd * 64;
        res.node->bw_wr = node_reg_ev.cas_wr * 64;
    }
}

void setup_table(int t, struct unity_lines *lines) {
    struct unity_line *line;
    int32_t socket_id = 0, channel_id = 0, line_num = 0;
    line_num = env.nr_socket * (1 + env.nr_channel) + 1;
    unity_alloc_lines(lines, line_num);
    for (socket_id = 0; socket_id < env.nr_socket; socket_id++) {
        char socket_name[32];
        snprintf(socket_name, 32, "%d", socket_id);

        line = unity_get_line(lines, (1 + env.nr_channel) * socket_id);
        unity_set_table(line, "imc_socket_event");
        unity_set_index(line, 0, "socket", socket_name);
        unity_set_value(line, 0, "rlat", res.socket[socket_id].rlat);
        unity_set_value(line, 1, "wlat", res.socket[socket_id].wlat);
        unity_set_value(line, 2, "avglat", res.socket[socket_id].avglat);
        unity_set_value(line, 3, "bw_rd", res.socket[socket_id].bw_rd);
        unity_set_value(line, 4, "bw_wr", res.socket[socket_id].bw_wr);

        for (channel_id = 0; channel_id < env.nr_channel; channel_id++) {
            char channel_name[32];
            snprintf(channel_name, 32, "%d", channel_id);

            line = unity_get_line(
                lines, (1 + env.nr_channel) * socket_id + 1 + channel_id);
            unity_set_table(line, "imc_channel_event");
            unity_set_index(line, 0, "socket", socket_name);
            unity_set_index(line, 1, "channel", channel_name);
            unity_set_value(line, 0, "rlat",
                            res.channel[socket_id][channel_id].rlat);
            unity_set_value(line, 1, "wlat",
                            res.channel[socket_id][channel_id].wlat);
            unity_set_value(line, 2, "avglat",
                            res.channel[socket_id][channel_id].avglat);
            unity_set_value(line, 3, "bw_rd",
                            res.channel[socket_id][channel_id].bw_rd);
            unity_set_value(line, 4, "bw_wr",
                            res.channel[socket_id][channel_id].bw_wr);
        }
    }

    line = unity_get_line(lines, line_num - 1);
    unity_set_table(line, "imc_node_event");
    unity_set_value(line, 0, "rlat", res.node->rlat);
    unity_set_value(line, 1, "wlat", res.node->wlat);
    unity_set_value(line, 2, "avglat", res.node->avglat);
    unity_set_value(line, 3, "bw_rd", res.node->bw_rd);
    unity_set_value(line, 4, "bw_wr", res.node->bw_wr);
}

void swap_regs() {
    /* swap data */
    reg_event **tmp = before.regs;
    before.regs = after.regs;
    after.regs = tmp;

    /* clear after data */
    free(after.regs);
    after.regs = alloc_record();
}

int call(int t, struct unity_lines *lines) {
    if (!env.init_succ) {
        return 0;
    }
    read_imc();
    calculate_metric();
#ifdef DEBUG
    print_result(res);
#endif

    setup_table(t, lines);
    swap_regs();
    /* reset before timestamp */
    before_ts = after_ts;
    return 0;
}

void deinit(void) { free_data(); }

#ifdef DEBUG
int main() {
    init(0);
    while (1) {
        sleep(1);
        call(0, 0);
    }

    deinit();
}
#endif
