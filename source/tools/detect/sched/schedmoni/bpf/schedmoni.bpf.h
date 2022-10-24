
#define TASK_RUNNING	0
#define _(P) ({typeof(P) val; __builtin_memset(&val, 0, sizeof(val)); bpf_probe_read(&val, sizeof(val), &P); val;})

#define GETARG_FROM_ARRYMAP(map,argp,type,member)({	\
	int i = 0;					\
	type retval;					\
	__builtin_memset(&retval, 0, sizeof(type));	\
	argp = bpf_map_lookup_elem(&map, &i);		\
	if (argp) {					\
		retval = _(argp->member);		\
	}						\
	retval;						\
	})

#define BPF_F_FAST_STACK_CMP	(1ULL << 9)
#define KERN_STACKID_FLAGS	(0 | BPF_F_FAST_STACK_CMP)

#define BIT_WORD(nr)	((nr) / BITS_PER_LONG)
#define BITS_PER_LONG	64

#define get_current_rqlen(p) ({			\
	int len = 0;				\
	struct cfs_rq *cfs;			\
	struct sched_entity *se, *parent;	\
	se = &p->se;				\
	for (int i = 0; i < 10; i++) {		\
		parent = _(se->parent);		\
		if (parent)			\
			se = parent;		\
		else				\
			break;			\
	}					\
	cfs = BPF_CORE_READ(se, cfs_rq);	\
	len = _(cfs->nr_running);		\
	len;					\
})

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 4);
	__type(key, u32);
	__type(value, struct args);
} argmap SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 10240);
	__type(key, u32);
	__type(value, struct enq_info);
} start SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events_rnslw SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events_nosch SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u32));
} events_irqof SEC(".maps");

struct bpf_map_def SEC("maps") stackmap = {
	.type = BPF_MAP_TYPE_STACK_TRACE,
	.key_size = sizeof(u32),
	.value_size = PERF_MAX_STACK_DEPTH * sizeof(u64),
	.max_entries = 1000,
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, MAX_MONI_NR);
	__type(key, u64);
	__type(value, struct latinfo);
} info_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct tm_info);
} tm_map SEC(".maps");

struct sched_switch_tp_args {
	struct trace_entry ent;
	char prev_comm[16];
	pid_t prev_pid;
	int prev_prio;
	long int prev_state;
	char next_comm[16];
	pid_t next_pid;
	int next_prio;
	char __data[0];
};
