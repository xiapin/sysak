#ifndef UNITY_UNCORE_IMC_H
#define UNITY_UNCORE_IMC_H

#include <linux/types.h>
#include <stdbool.h>
#include <linux/perf_event.h>
#include "../plugin_head.h"

int init(void* arg);
int call(int t, struct unity_lines* lines);
void deinit(void);

#define ULIMIT_RECOMMENDATION                                                 \
    ("try executing 'ulimit -n 1000000' to increase the limit on the number " \
     "of open files.\n")

typedef union CPUID_INFO {
    int array[4];
    struct {
        unsigned int eax, ebx, ecx, edx;
    } reg;
} CPUID_INFO;

enum INTEL_CPU_MODEL {
    NEHALEM_EP = 26,
    NEHALEM = 30,
    ATOM = 28,
    ATOM_2 = 53,
    CENTERTON = 54,
    BAYTRAIL = 55,
    AVOTON = 77,
    CHERRYTRAIL = 76,
    APOLLO_LAKE = 92,
    GEMINI_LAKE = 122,
    DENVERTON = 95,
    SNOWRIDGE = 134,
    CLARKDALE = 37,
    WESTMERE_EP = 44,
    NEHALEM_EX = 46,
    WESTMERE_EX = 47,
    SANDY_BRIDGE = 42,
    JAKETOWN = 45,
    IVY_BRIDGE = 58,
    HASWELL = 60,
    HASWELL_ULT = 69,
    HASWELL_2 = 70,
    IVYTOWN = 62,
    HASWELLX = 63,
    BROADWELL = 61,
    BROADWELL_XEON_E3 = 71,
    BDX_DE = 86,
    SKL_UY = 78,
    KBL = 158,
    KBL_1 = 142,
    CML = 166,
    CML_1 = 165,
    ICL = 126,
    ICL_1 = 125,
    RKL = 167,
    TGL = 140,
    TGL_1 = 141,
    ADL = 151,
    ADL_1 = 154,
    RPL = 0xb7,
    RPL_1 = 0xba,
    RPL_2 = 0xbf,
    RPL_3 = 0xbe,
    BDX = 79,
    KNL = 87,
    SKL = 94,
    SKX = 85,
    ICX_D = 108,
    ICX = 106,
    SPR = 143,
    END_OF_MODEL_LIST = 0x0ffff
};

#define MC_CH_PCI_PMON_CTL_EVENT(x) (x << 0)
#define MC_CH_PCI_PMON_CTL_UMASK(x) (x << 8)
#define MC_CH_PCI_PMON_CTL_RST (1 << 17)
#define MC_CH_PCI_PMON_CTL_EDGE_DET (1 << 18)
#define MC_CH_PCI_PMON_CTL_EN (1 << 22)
#define MC_CH_PCI_PMON_CTL_INVERT (1 << 23)
#define MC_CH_PCI_PMON_CTL_THRESH(x) (x << 24UL)
#define MC_CH_PCI_PMON_FIXED_CTL_RST (1 << 19)
#define MC_CH_PCI_PMON_FIXED_CTL_EN (1 << 22)
#define UNC_PMON_UNIT_CTL_FRZ_EN (1 << 16)
#define UNC_PMON_UNIT_CTL_RSV ((1 << 16) + (1 << 17))

#define RPQ_OCC 0
#define RPQ_INS 1
#define WPQ_OCC 2
#define WPQ_INS 3
#define CAS_RD	4
#define CAS_WR	5


#define BUF_SIZE 1024
#define MAX_IMC_ID 100
#define GENERAL_REG_NUM 6
#define FIXED_REG_NUM 1

typedef struct imc_event_t {
    struct perf_event_attr attr;
    int fd;
    int core_id;
    int pmu_id;
    bool fixed;
} imc_event;

typedef struct imc_reg_group_t {
    imc_event general_ev[GENERAL_REG_NUM];
    imc_event fixed_ev;
    int pmu_id;
} imc_reg_group;

typedef struct imc_pmu_t {
    imc_reg_group* reg_groups;
    int socket_id;
    int nr_grp;
} imc_pmu;

struct topology_ent {
    int64_t cpu_id;
    int64_t core_id;
    int64_t socket_id;
};

#endif
