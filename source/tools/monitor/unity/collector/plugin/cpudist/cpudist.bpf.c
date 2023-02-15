//
// Created by 廖肇燕 on 2023/2/7.
//

struct sched_switch_args {
    u16 type;
    u8  flag;
    u8  preeempt;
    u32 c_pid;
    char prev_comm[16];
    u32  prev_pid;
    u32 prev_prio;
    u64 prev_state;
    char next_comm[16];
    u32  next_pid;
    u32 next_prio;
};
SEC("tracepoint/sched/sched_switch")
int sched_switch_hook(struct sched_switch_args *args){
    u64 ts = bpf_ktime_get_ns();
    u64 *pv;
    u32 prev = args->prev_pid;
    u32 next = args->next_pid;

    if (next > 0) {
        bpf_map_update_elem(&start, &next, &ts, BPF_ANY);
    }
    pv = bpf_map_lookup_elem(&start, &prev);
    if (pv && ts > *pv) {
        hist10_push(&cpudist, (ts - *pv) / 1000);
    }
}
char _license[] SEC("license") = "GPL";

