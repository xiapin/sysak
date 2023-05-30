#include "vmlinux/common.h"
#include "net.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
//#include "vmlinux/vmlinux.h"


#define AF_INET		2	/* Internet IP Protocol 	*/
#define AF_INET6	10	/* IP version 6			*/
#define AF_UNKNOWN 0xff
#define	EINPROGRESS	115	/* Operation now in progress */
#define EEXIST 17
#define socklen_t size_t
#define WRAPPER_LEN 4
#define VEC_LEN 44
#define TASK_COMM_LEN 16
#define PROTO_LEN 128
#define MAX_CONNECT_ENTRIES 262144
#define MAX_PARAM_ENTRIES   8192
#define MONGO_HEADER_LEN 16
#define CONN_CLEANUP_NUMS 85

#define net_bpf_print(fmt, ...) 	\
({ 					\
	char ____fmt[] = fmt; 		\
	bpf_trace_printk(____fmt, sizeof(____fmt), 	\
			##__VA_ARGS__); 	\
}) 					\

const   uint64_t NSEC_PER_SEC = 1000000000L;
const   uint64_t USER_HZ = 100;
const   int ConnStatsBytesThreshold = 131072;
const   int ConnStatsPacketsThreshold = 128;

#define ntohs(x) (u16)__builtin_bswap16((u16)x)

#ifdef NET_TEST
/*  
 * below are maps for syscall doing something
 *
*/
struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __type(key, int);
    __type(value, int);
} test_map SEC(".maps");
#endif

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, int64_t);
    __uint(max_entries, TgidNum);
} config_tgid_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, u64);
    __uint(max_entries, NumProto);
} config_protocol_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, struct config_info_t);
    __uint(max_entries, 1);
} config_info_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct connect_info_t);
    __uint(max_entries, MAX_CONNECT_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} connect_info_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct conn_param_t);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} conn_param_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct accept_param_t);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} accept_param_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct close_param_t);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} close_param_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct data_param_t);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} write_param_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct data_param_t);
    __uint(max_entries, MAX_PARAM_ENTRIES);
    __uint(map_flags, BPF_F_NO_PREALLOC);
} read_param_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __type(key, int);
    __type(value, int);
} connect_data_events_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __type(key, int);
    __type(value, int);
} connect_ctrl_events_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
    __type(key, int);
    __type(value, int);
} connect_stats_events_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __type(key, u32);
    __type(value, struct conn_data_event_t);
    __uint(max_entries, 1);
} connect_data_events_heap SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u64);
    __type(value, struct conn_data_event_t);
    __uint(max_entries, MAX_PARAM_ENTRIES);
} trace_data_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, u64);
    __uint(max_entries, 5);
} http_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, u64);
    __uint(max_entries, 5);
} dns_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, u64);
    __uint(max_entries, 5);
} mysql_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, u32);
    __type(value, u64);
    __uint(max_entries, 5);
} redis_map SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
    __type(key, u32);
    __type(value, struct conn_stats_event_t);
    __uint(max_entries, 1);
} connect_stats_events_heap SEC(".maps");

/************  
 * below are func for bpf
 *
 ***********
*/

static __always_inline void set_addr_pair_by_sock(struct sock *sk, struct addr_pair_t *ap)
{
    BPF_CORE_READ_INTO(&ap->daddr, sk, __sk_common.skc_daddr);
    BPF_CORE_READ_INTO(&ap->dport, sk, __sk_common.skc_dport);
    BPF_CORE_READ_INTO(&ap->saddr, sk, __sk_common.skc_rcv_saddr);
    BPF_CORE_READ_INTO(&ap->sport, sk, __sk_common.skc_num);
}

static __always_inline enum support_tgid_e match_tgid(const uint32_t tgid)
{
  u32 index = TgidIndex;
  int64_t *config_tgid =  bpf_map_lookup_elem(&config_tgid_map, &index);

  if (config_tgid == NULL) {
    return TgidUndefine;
  }
  if (*config_tgid == tgid) {
    return TgidMatch;
  }
  if (*config_tgid < 0) {
    return TgidAll;
  }

  return TgidUnmatch;
}

static __always_inline uint64_t get_start_time()
{
  struct task_struct *task = (struct task_struct *)bpf_get_current_task();
  uint64_t gl_off = offsetof(struct task_struct, group_leader);
  struct task_struct *group_leader_ptr;
  bpf_probe_read(&group_leader_ptr, 
                 sizeof(struct task_struct *),
                 (uint8_t *)task + gl_off);

  uint64_t st_off = offsetof(struct task_struct, start_time);
  uint64_t start_time = 0;
  bpf_probe_read(&start_time, 
                 sizeof(uint64_t), 
                 (uint8_t *)group_leader_ptr + st_off);

  return start_time;
  // return nsec_to_clock_t(start_time);
}

static __always_inline uint64_t combine_tgid_fd(uint32_t tgid, int fd)
{
  return ((uint64_t)tgid << 32) | (uint32_t)fd;
}

static __always_inline void init_conn_id(uint32_t tgid, 
                                         int32_t fd, 
                                         struct connect_id_t *conn_id)
{
  conn_id->tgid = tgid;
  conn_id->fd = fd;
  conn_id->start = get_start_time();
}

static __always_inline void init_conn_info(uint32_t tgid, 
                                           int32_t fd, 
                                           struct connect_info_t *conn_info)
{
  init_conn_id(tgid, fd, &conn_info->conn_id);
  conn_info->role = IsUnknown;
  conn_info->addr.sa.sa_family = AF_UNKNOWN;
  conn_info->is_sample = true;
}

static __always_inline int32_t get_buf_32(const char *buf)
{
  uint32_t length;
  bpf_probe_read(&length, 4, buf);
  return bpf_ntohl(length);
}
static __always_inline int net_bpf_strncmp(const  char* src, const char* targ, size_t n)
{
	size_t i;
#pragma unroll
	for (i = 0; i < n; i++) {
		if (src[i] != targ[i]) {
			return 1;
		}
	}
	return 0;
}
static __always_inline int32_t get_buf_8(const char *buf)
{
  uint16_t val;
  bpf_probe_read(&val, 1, buf);
  return bpf_ntohs(val);
}

static __always_inline int32_t get_buf_16(const char *buf)
{
  uint16_t val;
  bpf_probe_read(&val, 2, buf);
  return bpf_ntohs(val);
}

static __always_inline enum support_type_e detect_http(const char *buf, size_t count)
{
  if (count < 16) {
    return TypeUnknown;
  }

  if (buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P') {
    return TypeResponse;
  }
  if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
    return TypeRequest;
  }
  if (buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'A' && buf[3] == 'D') {
    return TypeRequest;
  }
  if (buf[0] == 'P' && buf[1] == 'O' && buf[2] == 'S' && buf[3] == 'T') {
    return TypeRequest;
  }
  if (buf[0] == 'P' && buf[1] == 'U' && buf[2] == 'T') {
    return TypeRequest;
  }
  if (buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'E' && buf[4] == 'T' &&
      buf[5] == 'E') {
    return TypeRequest;
  }
  return TypeUnknown;
}

static __always_inline enum support_type_e detect_mysql(const char *buf, 
                                                        size_t count,
                                                        struct connect_info_t *conn_info)
{
  static const uint8_t ComQuery = 0x03;
  static const uint8_t ComConnect = 0x0b;
  static const uint8_t ComStmtPrepare = 0x16;
  static const uint8_t ComStmtExecute = 0x17;
  static const uint8_t ComStmtClose = 0x19;

  bool use_prev_buf = (conn_info->prev_count == 4) && 
                      (*((uint32_t *)conn_info->prev_buf) == count);
  if (use_prev_buf) {
    count += 4;
  }
  if (count < 5) {
    return TypeUnknown;
  }

  uint32_t len = use_prev_buf ? *((uint32_t*)conn_info->prev_buf) : *((uint32_t *)buf);
  len = len & 0x00ffffff;

  uint8_t seq = use_prev_buf ? conn_info->prev_buf[3] : buf[3];
  uint8_t com = use_prev_buf ? buf[0] : buf[4];
  if (seq != 0) {
    return TypeUnknown;
  }
  if (len == 0) {
    return TypeUnknown;
  }
  if (len > 10000) {
    return TypeUnknown;
  }

  if (com == ComConnect || 
      com == ComQuery || 
      com == ComStmtPrepare || 
      com == ComStmtExecute ||
      com == ComStmtClose) {
    return TypeRequest;
  }
  return TypeUnknown;
}

static __always_inline enum support_type_e defect_kafka_request(const char *buf)
{
  static const int NumAPIs = 62;
  static const int MaxAPIVersion = 12;

  const uint16_t request_API_key = get_buf_16(buf);
  if (request_API_key < 0 || request_API_key > NumAPIs) {
    return TypeUnknown;
  }

  const uint16_t request_API_version = get_buf_16(buf + 2);
  if (request_API_version < 0 || request_API_version > MaxAPIVersion) {
    return TypeUnknown;
  }

  const int32_t correlation_id = get_buf_32(buf + 4);
  if (correlation_id < 0) {
    return TypeUnknown;
  }
  return TypeRequest;
}

static __always_inline enum support_type_e detect_kafka(const char *buf, 
                                                        size_t count,
                                                        struct connect_info_t *conn_info)
{
  bool use_prev_buf = (conn_info->prev_count == 4) && 
                      ((size_t)get_buf_32(conn_info->prev_buf) == count);

  if (use_prev_buf) {
    count += 4;
  }
  static const int MinRequestLen = 12;
  if (count < MinRequestLen) {
    return TypeUnknown;
  }
  const int32_t message_size = use_prev_buf ? count : get_buf_32(buf) + 4;

  if (message_size < 0 || count != (size_t)message_size) {
    return TypeUnknown;
  }
  const char* request_buf = use_prev_buf ? buf : buf + 4;
  enum support_type_e result = defect_kafka_request(request_buf);

  if (use_prev_buf && result == TypeRequest && conn_info->protocol == ProtoUnknown) {
    conn_info->try_to_prepend = true;
  }
  return result;
}

/*
 *
 * 0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                      ID                       |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |QR|   Opcode  |AA|TC|RD|RA|   Z    |   RCODE   |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    QDCOUNT                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    ANCOUNT                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    NSCOUNT                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |                    ARCOUNT                    |
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 *
 */
static __always_inline enum support_type_e detect_dns(const char *buf, size_t count,
                                            struct protocol_type_t *proto)
{
  const int DNSHeaderSize = 12;
  const int MaxDNSMessageSize = 512;
  const int MaxNumRR = 25;

  if (count < DNSHeaderSize || count > MaxDNSMessageSize) {
    return TypeUnknown;
  }
  const uint8_t *ubuf = (const uint8_t *)buf;
  uint16_t flags = (ubuf[2] << 8) + ubuf[3];
  uint16_t num_questions = (ubuf[4] << 8) + ubuf[5];
  uint16_t num_answers = (ubuf[6] << 8) + ubuf[7];
  uint16_t num_auth = (ubuf[8] << 8) + ubuf[9];
  uint16_t num_addl = (ubuf[10] << 8) + ubuf[11];
  bool qr = (flags >> 15) & 0x1;
  uint8_t opcode = (flags >> 11) & 0xf;
  uint8_t zero = (flags >> 6) & 0x1;
  if (zero != 0) {
    return TypeUnknown;
  }
  if (opcode != 0) {
    return TypeUnknown;
  }
  if (num_questions == 0 || num_questions > 10) {
    return TypeUnknown;
  }
  uint32_t num_rr = num_questions + num_answers + num_auth + num_addl;
  if (num_rr > MaxNumRR) {
    return TypeUnknown;
  }
  proto->rescode = flags & 0x0000F;
  return (qr == 0) ? TypeRequest : TypeResponse;
}

static __always_inline bool is_redis(const char *buf, size_t count)
{
  if (count < 3) {
    return false;
  }
  char first_byte;
  bpf_probe_read(&first_byte, 1, &buf[0]);
  if ( 
      first_byte != '+' &&
      first_byte != '-' &&
      first_byte != ':' &&
      first_byte != '$' &&
      first_byte != '*') {
    return false;
  }
  char last_byte;
  uint32_t lcount = count;
  bpf_probe_read(&last_byte, 1, &buf[lcount -2]);
  if (last_byte != '\r') {
    return false;
  }
  bpf_probe_read(&last_byte, 1, &buf[lcount -1]);
  if (last_byte != '\n') {
    return false;
  }
  return true;
}

static __always_inline enum support_type_e detect_pgsql_start_mesg(const char *buf,
						      size_t count)
{
	int min_msg_len = 12;
	int max_msg_len = 10240;
	char pgsql[] = "\x00\x03\x00\x00";
	int len;
	size_t i;

	if (count < min_msg_len) {
		return TypeUnknown;
	}
	len = get_buf_32(buf);

	if (len < min_msg_len || len > max_msg_len) {
		return TypeUnknown;
	}
	if (net_bpf_strncmp(buf + 4, pgsql, 4) != 0) {
		return TypeUnknown;
	}
#pragma unroll
	for (i = 0; i < 3; i++) {
		if (*(buf + 8 + i) < 'A') {
			return TypeUnknown;
		}
	}

	return TypeRequest;
}

static __always_inline enum support_type_e detect_pgsql_query_mesg(const char *buf,
						      size_t count)
{
	uint8_t tag_q = 'Q';
	int32_t min_payload_len = 8;
	int32_t max_payload_len = 30000;
	int32_t len;

	if (*buf != tag_q) {
		return TypeUnknown;
	}

	len = get_buf_32(buf + 1);
	if (len < min_payload_len || len > max_payload_len) {
		return TypeUnknown;
	}

	/*
	if ((len + 1 <= (int)count) && (buf[len] != '\0')) {
		return TypeUnknown;
	}
	*/

	return TypeRequest;
}

static __always_inline enum support_type_e detect_pgsql_reglr_mesg(const char *buf,
						      size_t count)
{
	int min_mesg_len = 1 + sizeof(int32_t);

	if (count < min_mesg_len) {
		return TypeUnknown;
	}
	return detect_pgsql_query_mesg(buf, count);
}

static __always_inline enum support_type_e detect_pgsql(const char *buf,
						      size_t count)
{
	enum support_type_e type = detect_pgsql_start_mesg(buf, count);
	if (type != TypeUnknown) {
		return type;
	}
	return detect_pgsql_reglr_mesg(buf, count);
}

enum mongo_type_e {
	MGUpdate = 2001,
	MGInsert = 2002,
	MGReserved = 2003,
	MGQuery = 2004,
	MGGetMore = 2005,
	MGDelete = 2006,
	MGKillGurs = 2007,
	MGCompres = 2012,
	MGMsg = 2013,
};

static __always_inline enum support_type_e detect_mongo(const char *buf,
						      size_t count)
{
	int32_t *info = (int32_t*)buf;
	int32_t mes_len = info[0];
	int32_t req_id = info[1];
	int32_t respon = info[2];
	int32_t opcode = info[3];

	if (count < MONGO_HEADER_LEN) {
		return TypeUnknown;
	}

	if (opcode == MGUpdate || opcode == MGInsert || opcode == MGReserved || opcode == MGQuery
			|| opcode == MGGetMore || opcode == MGDelete || opcode == MGKillGurs
			|| opcode == MGCompres || opcode == MGMsg) {
		if (respon == 0) {
			return TypeRequest;
		}
	}
	return TypeUnknown;
}

/*
 * keep alive package
 * 1 byte: magic:0x0C
 * 1 byte: req:0 resp:1
 *
 * normal package
 * 1 byte: magic:0x0E
 * 1 byte: version
 * 1 byte: req:0 resp:1
 */
static __always_inline enum support_type_e detect_hsf(const char *buf,
						      size_t count)
{
	uint8_t magic_n = 0x0E; // for normal package
	uint8_t magic_a = 0x0C; // for keep alive package

	if (magic_n == get_buf_8(buf)) {
		if (get_buf_8(buf + 1) == 0x0) {
			return TypeRequest;
		}
		if (get_buf_8(buf + 1) == 0x1) {
			return TypeResponse;
		}
	}
	if (magic_a == get_buf_8(buf)) {
		if (get_buf_8(buf + 2) == 0x0) {
			return TypeRequest;
		}
		if (get_buf_8(buf + 2) == 0x1) {
			return TypeResponse;
		}
	}
	return TypeUnknown;
}

/*
 * 16bits: 0xdabb
 * 1bit: req:1 resp:0
 */
static __always_inline enum support_type_e detect_dubbo(const char *buf,
						      size_t count)
{
#define REQ_RESP_BIT 0x1 << 16
	uint16_t magic = 0xdabb;

	if (magic == get_buf_16(buf)) {
		if (get_buf_32(buf) & REQ_RESP_BIT) {
			return TypeRequest;
		} else {
			return TypeResponse;
		}
	}
	return TypeUnknown;
}

static __always_inline struct protocol_type_t detect_proto(const char *buf, 
                                                           size_t count,
                                                           struct connect_info_t *conn_info)
{
  struct protocol_type_t proto_type;
  proto_type.protocol = ProtoUnknown;
  proto_type._type = TypeUnknown;
  proto_type.rescode = -1;
  conn_info->try_to_prepend = false;
  char pbuf[PROTO_LEN];
  bpf_probe_read(pbuf, PROTO_LEN, buf);

  if ((proto_type._type = detect_http(pbuf, count)) != TypeUnknown) {
    proto_type.protocol = ProtoHTTP;
  } else if ((proto_type._type = detect_mysql(pbuf, count, conn_info)) != TypeUnknown) {
    proto_type.protocol = ProtoMySQL;
  } else if ((proto_type._type = detect_kafka(pbuf, count, conn_info)) != TypeUnknown) {
    proto_type.protocol = ProtoKafka;
    /* special handle for dns */
  } else if ((proto_type._type = detect_dns(pbuf, count, &proto_type)) != TypeUnknown) {
    proto_type.protocol = ProtoDNS;
  } else if (is_redis(pbuf, count)) {
    proto_type.protocol = ProtoRedis;
  } else if ((proto_type._type = detect_mongo(pbuf, count)) != TypeUnknown) {
	proto_type.protocol = ProtoMongo;
  } else if ((proto_type._type = detect_pgsql(pbuf, count)) != TypeUnknown) {
	proto_type.protocol = ProtoPGSQL;
  } else if ((proto_type._type = detect_dubbo(pbuf, count)) != TypeUnknown) {
	proto_type.protocol = ProtoDubbo;
  } else if ((proto_type._type = detect_hsf(pbuf, count)) != TypeUnknown) {
	proto_type.protocol = ProtoHSF;
  }

  conn_info->prev_count = count;
  if (count == 4) {
    conn_info->prev_buf[0] = pbuf[0];
    conn_info->prev_buf[1] = pbuf[1];
    conn_info->prev_buf[2] = pbuf[2];
    conn_info->prev_buf[3] = pbuf[3];
  }

  return proto_type;
}

static __always_inline void update_proto_type(struct connect_info_t *conn_info,
                                              enum support_direction_e direction, 
                                              const char *buf,
                                              size_t count)
{
  if (conn_info == NULL) {
    return;
  }
  conn_info->total_bytes_for_proto += 1;
  struct protocol_type_t proto_type = detect_proto(buf, count, conn_info);

  if (proto_type.protocol == ProtoUnknown) {
    return;
  }
  if (conn_info->protocol == ProtoUnknown) {
    conn_info->protocol = proto_type.protocol;
  }
  conn_info->_type = proto_type._type;
  conn_info->proto_rescode = proto_type.rescode;
  if (conn_info->role == IsUnknown && proto_type._type != TypeUnknown) {
    bool isrole = (direction == DirEgress) ^ (proto_type._type == TypeResponse);
    conn_info->role = isrole ? IsClient : IsServer;
  }
}

static __always_inline void get_sock_addr(struct connect_info_t *conn_info,
                                          const struct socket *socket)
{
  struct sock *sk = NULL;
  bpf_probe_read_kernel(&sk, sizeof(sk), &socket->sk);
  struct sock_common *sk_common = &sk->__sk_common;
  uint16_t family = -1;
  uint16_t port = -1;

  bpf_probe_read_kernel(&family, sizeof(family), &sk_common->skc_family);
  bpf_probe_read_kernel(&port, sizeof(port), &sk_common->skc_dport);

  conn_info->addr.sa.sa_family = family;
  if (family == AF_INET) {
    conn_info->addr.in4.sin_port = port;
    uint32_t *addr = &conn_info->addr.in4.sin_addr.s_addr;
    bpf_probe_read_kernel(addr, sizeof(*addr), &sk_common->skc_daddr);
  } else if (family == AF_INET6) {
    conn_info->addr.in6.sin6_port = port;
    struct in6_addr *addr = &conn_info->addr.in6.sin6_addr;
    bpf_probe_read_kernel(addr, sizeof(*addr), &sk_common->skc_v6_daddr);
  }
}
static __always_inline bool need_trace_family(uint16_t family)
{
  return family == AF_UNKNOWN || family == AF_INET || family == AF_INET6;
}

static __always_inline struct conn_data_event_t *add_data_event(enum support_syscall_e src_fn, 
                                                                enum support_direction_e direction,
                                                                const struct connect_info_t *conn_info)
{
  uint32_t index = 0;
  struct conn_data_event_t *event = bpf_map_lookup_elem(&connect_data_events_heap, &index);
  if (event == NULL) {
    return NULL;
  }

  event->attr.ts = bpf_ktime_get_ns();
  event->attr.syscall_func = src_fn;
  event->attr.direction = direction;
  event->attr.conn_id = conn_info->conn_id;
  event->attr.protocol = conn_info->protocol;
  event->attr.role = conn_info->role;
  event->attr.pos = (direction == DirEgress) ? conn_info->wr_bytes : conn_info->rd_bytes;
  event->attr.try_to_prepend = conn_info->try_to_prepend;
  event->attr._type = conn_info->_type;
  event->attr.proto_rescode = conn_info->proto_rescode;
  bpf_probe_read(&event->attr.length_header, 4, conn_info->prev_buf);
  event->attr.addr = conn_info->addr;
  bpf_get_current_comm(&event->attr.comm, sizeof(event->attr.comm));
  return event;
}

static __always_inline struct conn_stats_event_t *add_conn_stats(struct connect_info_t *conn_info)
{
  uint32_t index = 0; 
  struct conn_stats_event_t *event = bpf_map_lookup_elem(&connect_stats_events_heap, &index);
  if (event == NULL) {
    return NULL;
  }

  event->conn_id = conn_info->conn_id;
  event->addr = conn_info->addr;
  event->role = conn_info->role;
  event->wr_bytes = conn_info->wr_bytes;
  event->rd_bytes = conn_info->rd_bytes;
  event->wr_pkts = conn_info->wr_pkts;
  event->rd_pkts = conn_info->rd_pkts;
  event->last_output_rd_pkts = conn_info->last_output_rd_pkts;
  event->last_output_wr_pkts = conn_info->last_output_wr_pkts;
  event->last_output_rd_bytes = conn_info->last_output_rd_bytes;
  event->last_output_wr_bytes = conn_info->last_output_wr_bytes;
  event->conn_events = 0;
  event->ts = bpf_ktime_get_ns();
  conn_info->last_output_time = event->ts;
  return event;
}

static __always_inline void filter_data_sample(struct config_info_t *config_info,
						struct connect_info_t *conn_info)
{
	if (config_info->data_sample == DATA_SAMPLE_ALL) {
		return;
	}
	if (bpf_ktime_get_ns() % DATA_SAMPLE_ALL < config_info->data_sample) {
		return;
	}
	conn_info->is_sample = false;
}

static __always_inline int filter_self(struct config_info_t *config_info,
					struct connect_info_t *conn_info)
{
	if (config_info->self_pid < 0) {
		return 0;
	}

	if (conn_info->conn_id.tgid == config_info->self_pid) {
		return 1;
	}

	return 0;
}

static __always_inline int filter_port(struct config_info_t *config_info,
					struct connect_info_t *conn_info)
{
	uint16_t family = -1;
	uint16_t port = -1;

	if (config_info->port < 0) {
		return 0;
	}

	family = conn_info->addr.sa.sa_family;
	if (family == AF_INET) {
		port = conn_info->addr.in4.sin_port;
	} else if (family == AF_INET6) {
		port = conn_info->addr.in6.sin6_port;
	}

	if (config_info->port == ntohs(port)) {
		return 0;
	}
	return 1;
}

static __always_inline int filter_config_info(struct connect_info_t *conn_info)
{
	struct config_info_t *config_info;
	uint32_t key = 0;

	config_info = bpf_map_lookup_elem(&config_info_map, &key);
	if (!config_info) {
		return 0;
	}
  /* not support now
	if (filter_port(config_info, conn_info)) {
		return 1;
	}
  */
	if (filter_self(config_info, conn_info)) {
		return 1;
	}
	filter_data_sample(config_info, conn_info);
	return 0;
}

static __always_inline struct connect_info_t *build_conn_info(uint32_t tgid, int32_t fd)
{
  uint64_t tgid_fd = combine_tgid_fd(tgid, fd);
  struct connect_info_t new_conn_info = {};
  init_conn_info(tgid, fd, &new_conn_info);
	long err;

  struct connect_info_t *tmp = (struct connect_info_t *)bpf_map_lookup_elem(&connect_info_map, &tgid_fd);
  if (tmp)
    return tmp;

  if (filter_config_info(&new_conn_info)) {
	  return NULL;
  }
  err = bpf_map_update_elem(&connect_info_map, &tgid_fd, &new_conn_info, BPF_NOEXIST);
  if (err && err != -EEXIST)
    return NULL;

  return (struct connect_info_t *)bpf_map_lookup_elem(&connect_info_map, &tgid_fd);
}

static __always_inline bool need_trace_protocol(const struct connect_info_t *conn_info)
{
  if (conn_info->protocol == ProtoUnknown) {
    return false;
  }

  uint32_t protocol = conn_info->protocol;
  uint64_t traced = 0;
  uint64_t config;
  uint64_t *tmp = (uint64_t *)bpf_map_lookup_elem(&config_protocol_map, &protocol);
  if (tmp != NULL)
    bpf_probe_read(&config, sizeof(uint64_t), tmp);

  if (bpf_map_update_elem(&config_protocol_map, &protocol, &traced, BPF_NOEXIST) == 0) {
    tmp = (uint64_t *)bpf_map_lookup_elem(&config_protocol_map, &protocol);
    bpf_probe_read(&config, sizeof(uint64_t), tmp);
  }

  // return config & conn_info->role;
  return config;
}

static __always_inline void add_one_conn(struct trace_event_raw_sys_exit *ctx, 
                                         const struct sockaddr *addr,
                                         const struct socket *socket, 
                                         struct tg_info_t *tg_role)
{
  struct connect_info_t conn_info = {};
  uint32_t tgid = tg_role->tgid;
  int32_t fd = tg_role->fd;
  enum support_role_e role = tg_role->role;

  init_conn_info(tgid, fd, &conn_info);
  if (addr != NULL) {
    bpf_probe_read(&conn_info.addr, sizeof(struct sockaddr), addr); 
  } else if (socket != NULL) {
    get_sock_addr(&conn_info, socket);
  }
  if (filter_config_info(&conn_info)) {
	  return;
  }

  conn_info.role = role;
  uint64_t tgid_fd = combine_tgid_fd(tgid, fd);
  //net_bpf_print("start ====add_conn\n");
  bpf_map_update_elem(&connect_info_map, &tgid_fd, &conn_info, BPF_ANY);
  if (!need_trace_family(conn_info.addr.sa.sa_family)) {
    return;
  }

  //net_bpf_print("end ====add_conn\n");
  struct conn_ctrl_event_t ctrl_event = {};
  ctrl_event.type = EventConnect;
  ctrl_event.ts = bpf_ktime_get_ns();
  ctrl_event.conn_id = conn_info.conn_id;
  ctrl_event.connect.addr = conn_info.addr;
  ctrl_event.connect.role = conn_info.role;
  if (conn_info.is_sample) {
    /*
	  bpf_perf_event_output(ctx, &connect_ctrl_events_map, BPF_F_CURRENT_CPU,
			&ctrl_event, sizeof(struct conn_ctrl_event_t));
    */
  }
}

static __always_inline void output_conn_stats(struct trace_event_raw_sys_exit *ctx, 
                                              struct connect_info_t *conn_info,
                                              enum support_direction_e direction, 
                                              ssize_t return_bytes)
{
  switch (direction) {
    case DirEgress:
      conn_info->wr_bytes += return_bytes;
      conn_info->wr_pkts++;
      break;
    case DirIngress:
      conn_info->rd_bytes += return_bytes;
      conn_info->rd_pkts++;
      break;
  }

  uint64_t total_bytes = conn_info->wr_bytes + conn_info->rd_bytes;
  uint32_t total_pkts = conn_info->wr_pkts + conn_info->rd_pkts;
  
  bool real_threshold = (total_bytes >= conn_info->last_output_rd_bytes
					+ conn_info->last_output_wr_bytes
					+ ConnStatsBytesThreshold) || (total_pkts >= conn_info->last_output_rd_pkts
          + conn_info->last_output_wr_pkts
          + ConnStatsPacketsThreshold);
  if (real_threshold) {
    struct conn_stats_event_t *event = add_conn_stats(conn_info);
    if (event != NULL) {
      /*
      bpf_perf_event_output(ctx, &connect_stats_events_map, BPF_F_CURRENT_CPU, event, sizeof(struct conn_stats_event_t));
      */
    }
    conn_info->last_output_wr_bytes = conn_info->wr_bytes;
    conn_info->last_output_rd_bytes = conn_info->rd_bytes;
    conn_info->last_output_wr_pkts = conn_info->wr_pkts;
    conn_info->last_output_rd_pkts = conn_info->rd_pkts;
  }
}

static __always_inline void add_close_event(struct trace_event_raw_sys_exit *ctx, struct connect_info_t *conn_info)
{
  struct conn_ctrl_event_t ctrl_event = {};
  ctrl_event.type = EventClose;
  ctrl_event.ts = bpf_ktime_get_ns();
  ctrl_event.conn_id = conn_info->conn_id;
  ctrl_event.close.rd_bytes = conn_info->rd_bytes;
  ctrl_event.close.wr_bytes = conn_info->wr_bytes;
  if (conn_info->is_sample) {
    /*
	  bpf_perf_event_output(ctx, &connect_ctrl_events_map, BPF_F_CURRENT_CPU,
			&ctrl_event, sizeof(struct conn_ctrl_event_t));
    */
  }
}

static __always_inline void trace_exit_connect(struct trace_event_raw_sys_exit *ctx, 
                                               uint64_t id,
                                               const struct conn_param_t *conn_param) 
{
  uint32_t tgid = id >> 32;
  int ret = ctx->ret;

  if (match_tgid(tgid) == TgidUnmatch || conn_param->fd < 0) {
    return;
  }
  if (ret < 0 && ret != -EINPROGRESS) {
    return;
  }

  struct tg_info_t tg_role = { tgid, conn_param->fd, IsClient };
  add_one_conn(ctx, conn_param->addr, NULL, &tg_role);
}

static __always_inline void trace_reserve_conn(struct trace_event_raw_sys_exit *ctx, 
                                               uint64_t id,
                                               const struct conn_param_t *conn_param)
{
  uint32_t tgid = id >> 32;
  if (match_tgid(tgid) == TgidUnmatch) {
    return;
  }
  if (conn_param->fd < 0) {
    return;
  }

  uint64_t tgid_fd = combine_tgid_fd(tgid, conn_param->fd);
  struct connect_info_t *conn_info = bpf_map_lookup_elem(&connect_info_map, &tgid_fd);
  if (conn_info != NULL) {
    return;
  }
  struct tg_info_t tg_role = {tgid, conn_param->fd, IsUnknown};
  add_one_conn(ctx, conn_param->addr, NULL, &tg_role);
}

static __always_inline void trace_exit_close(struct trace_event_raw_sys_exit *ctx, 
                                             uint64_t id,
                                             const struct close_param_t *close_param)
{
  uint32_t tgid = id >> 32;
  int ret = ctx->ret;

  if (ret < 0 || close_param->fd < 0) {
    return;
  }
  if (match_tgid(tgid) == TgidUnmatch) {
    return;
  }

  uint64_t tgid_fd = combine_tgid_fd(tgid, close_param->fd);
  struct connect_info_t *conn_info = bpf_map_lookup_elem(&connect_info_map, &tgid_fd);
  if (conn_info == NULL) {
    return;
  }
  /* 未等到read配对,超时提前close的情况此处需要清理map中记录的数据
   * Todo: 此处是否需要将时延上报
   */
  struct conn_data_event_t *data = bpf_map_lookup_elem(&trace_data_map, &tgid_fd);
  if (data) {
    bpf_map_delete_elem(&trace_data_map, &tgid_fd);
  }

  if (need_trace_family(conn_info->addr.sa.sa_family) || 
                        conn_info->wr_bytes != 0 ||
                        conn_info->rd_bytes != 0) {

    add_close_event(ctx, conn_info);
    if (conn_info->last_output_rd_pkts + conn_info->last_output_wr_pkts != conn_info->rd_pkts + conn_info->wr_pkts) {
      struct conn_stats_event_t *stats_event = add_conn_stats(conn_info);
      if (stats_event != NULL) {
        stats_event->conn_events = stats_event->conn_events | StatusClose;
        /*
        bpf_perf_event_output(ctx, &connect_stats_events_map, BPF_F_CURRENT_CPU, stats_event, sizeof(struct conn_stats_event_t));
        */
      }
    }
  }

  bpf_map_delete_elem(&connect_info_map, &tgid_fd);
}

static __always_inline void trace_exit_accept(struct trace_event_raw_sys_exit *ctx, 
                                              uint64_t id,
                                              const struct accept_param_t *accept_param)
{
  uint32_t tgid = id >> 32;
  int newfd = ctx->ret;

  if (match_tgid(tgid) == TgidUnmatch) {
    return;
  }
  if (newfd < 0) {
    return;
  }

  struct tg_info_t tg_role = { tgid, newfd, IsServer };
  add_one_conn(ctx, accept_param->addr, accept_param->accept_socket, &tg_role);
}


static inline void addDist(int k, enum support_proto_e proto)
{
  u64 *pv;
  switch (proto) {
    case ProtoHTTP:
      pv = bpf_map_lookup_elem(&http_map, &k);
      if (pv) {
        __sync_fetch_and_add(pv, 1);
      }
      break;
    case ProtoDNS:
      pv = bpf_map_lookup_elem(&dns_map, &k);
      if (pv) {
        __sync_fetch_and_add(pv, 1);
      }
      break;
    case ProtoMySQL:
      pv = bpf_map_lookup_elem(&mysql_map, &k);
      if (pv) {
        __sync_fetch_and_add(pv, 1);
      }
      break;
    case ProtoRedis:
      pv = bpf_map_lookup_elem(&redis_map, &k);
      if (pv) {
        __sync_fetch_and_add(pv, 1);
      }
      break;
    defaults:
      break;
  } 
}

static __always_inline void checkUs(uint64_t delta, enum support_proto_e proto) {
  /* 10ms */
  if (delta < 10000) {
    addDist(0, proto);
  /* 100ms */
  } else if (delta < 100000) {
    addDist(1, proto);
  /* 500ms */
  } else if (delta < 500000) {
    addDist(2, proto);
  /* 1s */
  } else if (delta < 1000000) {
    addDist(3, proto);
  } else {
    addDist(4, proto);
  }
}

static inline int check_inner(unsigned int ip)
{
    int i;
    const unsigned int array[3][2] = {
            {0x0000000A, 0x000000ff},
            {0x000010AC, 0x0000f0ff},
            {0x0000A8C0, 0x0000ffff},
    };

    if (ip == 0) {
        return 1;
    }
    #pragma unroll 3
    for (i =0; i < 3; i ++) {
        if ((ip & array[i][1]) == array[i][0]) {
            return 1;
        }
    }
    return 0;
}

static int dns_notify_err_res(void *ctx, struct conn_data_event_t *req_data,
                struct conn_data_event_t *resp_data, size_t copied)
{
  /* -1 is the init code */
  if (resp_data->attr.proto_rescode > 0 && resp_data->attr._type == TypeResponse) {
      bpf_perf_event_output(ctx, &connect_data_events_map, BPF_F_CURRENT_CPU,
        resp_data, sizeof(resp_data->attr) + copied);
  }
  return 0;
}

static int read_write_match(void *ctx, struct conn_data_event_t *event, size_t copied)
{
  uint64_t tgid_fd;
  struct attr_t attr = {};
  enum support_syscall_e func = event->attr.syscall_func;
  enum support_proto_e proto = event->attr.protocol;
  uint16_t family = event->attr.addr.sa.sa_family;
  unsigned int ip;

  /* Only support ipv4, except for dns */
  if (family != AF_INET && proto != ProtoDNS) {
    return 0;
  }

  tgid_fd = combine_tgid_fd(event->attr.conn_id.tgid, event->attr.conn_id.fd);
  /* read update */
  if (func == FuncWrite || func == FuncSecuritySendMsg || func == FuncSendMsg || func == FuncSend
      || func == FuncSendTo) {
    struct conn_data_event_t * data = bpf_map_lookup_elem(&trace_data_map, &tgid_fd);
    if (!data) {
      //net_bpf_print("a, tid:%u, proto:%d\n", (uint32_t)tgid, proto);
      bpf_map_update_elem(&trace_data_map, &tgid_fd, event, BPF_ANY);
    }
    return 0;
  }
  /* write update */
  struct conn_data_event_t * data = bpf_map_lookup_elem(&trace_data_map, &tgid_fd);
  if (!data)
    return 0;

  bpf_probe_read(&attr, sizeof(struct attr_t), data);
  uint32_t ts = event->attr.ts - attr.ts;
  ip = attr.addr.in4.sin_addr.s_addr;

  checkUs(ts/1000, proto);
  /*
  * todo: optimize it later, 只将超时的公网ip地址上报
  */
  event->attr.ts = ts;
  if (event->attr.protocol == ProtoDNS) {
    dns_notify_err_res(ctx, data, event, copied);
  }

  if (ts > NSEC_PER_SEC && !check_inner(ip)) {
    bpf_perf_event_output(ctx, &connect_data_events_map, BPF_F_CURRENT_CPU,
      event, sizeof(event->attr) + copied);
  }

  //net_bpf_print("b, tid:%u,p:%d,f:%d\n", (uint32_t)tgid, proto, func);
  //net_bpf_print("ts:%u\n",ts);
  bpf_map_delete_elem(&trace_data_map, &tgid_fd);

  return 0;
}

static void perf_output_user(struct trace_event_raw_sys_exit *ctx, 
                                             const enum support_direction_e direction,
                                             const char *buf, size_t buf_size,
                                             struct connect_info_t *conn_info,
                                             struct conn_data_event_t *event)
{
  event->attr.org_msg_size = buf_size;
  if (buf_size == 0) {
    return;
  }

  size_t buf_size_minus_1 = buf_size - 1;
  asm volatile("" : "+r"(buf_size_minus_1) :);
  buf_size = buf_size_minus_1 + 1;
  size_t copied = 0;

  if (buf_size_minus_1 < CONN_DATA_MAX_SIZE) {
    bpf_probe_read(&event->msg, buf_size, buf);
    copied = buf_size;
  } else {
    bpf_probe_read(&event->msg, CONN_DATA_MAX_SIZE, buf);
    copied = CONN_DATA_MAX_SIZE;
  }

  if (copied > 0) {
    event->attr.msg_buf_size = copied;
    /*
	  bpf_perf_event_output(ctx, &connect_data_events_map, BPF_F_CURRENT_CPU,
			event, sizeof(event->attr) + copied);
    */
    read_write_match(ctx, event, copied);
  }
}

static __always_inline void output_buf(struct trace_event_raw_sys_exit *ctx,
                                               const enum support_direction_e direction, 
                                               const char *buf,
                                               const size_t buf_size, 
                                               struct connect_info_t *conn_info,
                                               struct conn_data_event_t *event)
{
  int bytes_sent = 0;
  unsigned int i;

#pragma unroll
  for (i = 0; i < WRAPPER_LEN; ++i) {
    const int bytes_remain = buf_size - bytes_sent;
    bool remain = bytes_remain > CONN_DATA_MAX_SIZE && (i != WRAPPER_LEN - 1);
    const size_t current_size = remain ? CONN_DATA_MAX_SIZE : bytes_remain;
    perf_output_user(ctx, direction, buf + bytes_sent, current_size, conn_info, event);
    bytes_sent += current_size;
    event->attr.pos += current_size;
  }
}

static __always_inline void output_iovec(struct trace_event_raw_sys_exit *ctx,
                                        const enum support_direction_e direction,
                                        const struct iovec *iov, 
                                        const size_t iovlen,
                                        const size_t total_size, 
                                        struct connect_info_t *conn_info,
                                        struct conn_data_event_t *event)
{
  int bytes_sent = 0;
#pragma unroll
  for (int i = 0; i < VEC_LEN && i < iovlen; ++i) {
    // Fix unroll failed in clang12
    if (bytes_sent < total_size)
	    continue;
    else
	    break;
    struct iovec iov_cpy;
    bpf_probe_read(&iov_cpy, sizeof(struct iovec), &iov[i]);
    const int bytes_remain = total_size - bytes_sent;
    bool remain = iov_cpy.iov_len < bytes_remain;
    const size_t iov_size = remain ? iov_cpy.iov_len : bytes_remain;
    perf_output_user(ctx, direction, iov_cpy.iov_base, iov_size, conn_info, event);
    bytes_sent += iov_size;
    event->attr.pos += iov_size;
  }
}

static __always_inline void trace_exit_data(struct trace_event_raw_sys_exit *ctx, 
                                            uint64_t id,
                                            const enum support_direction_e direction,
                                            const struct data_param_t *data_param, 
                                            ssize_t return_bytes, 
                                            bool vecs)
{
  uint32_t tgid = id >> 32;

  if (!vecs && data_param->buf == NULL) {
    return;
  }
  if (vecs && (data_param->iov == NULL || data_param->iovlen <= 0)) {
    return;
  }
  if (data_param->fd < 0 || return_bytes <= 0) {
    return;
  }

  enum support_tgid_e matched = match_tgid(tgid);
  if (matched == TgidUnmatch) {
    //net_bpf_print("==you exit_data3\n");
    return;
  }

  struct connect_info_t *conn_info = build_conn_info(tgid, data_param->fd);
  if (conn_info == NULL) {
    return;
  }
  if (!need_trace_family(conn_info->addr.sa.sa_family)) {
    return;
  }

  output_conn_stats(ctx, conn_info, direction, return_bytes);
  if (!conn_info->is_sample) {
    return;
  }
  if (!vecs) {
    update_proto_type(conn_info, direction, data_param->buf, return_bytes);
  } else {
    struct iovec iov_cpy;
    bpf_probe_read(&iov_cpy, sizeof(struct iovec), &data_param->iov[0]);
    const size_t buf_size = iov_cpy.iov_len < return_bytes ? iov_cpy.iov_len : return_bytes;
    update_proto_type(conn_info, direction, iov_cpy.iov_base, buf_size);
  }

  if (need_trace_protocol(conn_info) || matched == TgidMatch) {
    struct conn_data_event_t *event = add_data_event(data_param->syscall_func, direction, conn_info);
    if (event == NULL) {
       return;
    }

    if (!vecs) {
      output_buf(ctx, direction, data_param->buf, return_bytes, conn_info, event);
    } else {
      output_iovec(ctx, direction, data_param->iov, data_param->iovlen, return_bytes, conn_info, event);
    }
  }
  return;
}

#ifdef NET_TEST
static __always_inline void test_bpf_syscall(void *ctx, 
                                             uint64_t id, 
                                             int32_t fd, 
                                             const struct sockaddr *addr, 
                                             int32_t ret, 
                                             uint32_t funcid)
{

  struct test_data tmp_nd = {};
  union sockaddr_t test_addr;
  uint16_t family = 0;
  uint16_t port = 0;
  uint32_t u4addr = 0;

  if (addr) {
    bpf_probe_read(&test_addr, sizeof(struct sockaddr), addr);
    family = test_addr.sa.sa_family;
    port = test_addr.in4.sin_port;
    u4addr = test_addr.in4.sin_addr.s_addr;
  } else {
  #if 0
  tmp_nd.pid = id >> 32;
  tmp_nd.funcid = funcid;
  // tmp_nd.fd = lfd;
  tmp_nd.ap.dport = port;
  tmp_nd.ap.daddr = u4addr;
  tmp_nd.family = family;
  tmp_nd.ret_val = ret;

  bpf_get_current_comm(&tmp_nd.com, TASK_COMM_LEN);
  bpf_perf_event_output(ctx, &test_map, BPF_F_CURRENT_CPU, &tmp_nd, sizeof(struct test_data));
  #endif
  return;
  } 
  
  tmp_nd.pid = id >> 32;
  tmp_nd.funcid = funcid;
  tmp_nd.fd = fd;
  tmp_nd.ap.dport = port;
  tmp_nd.ap.daddr = u4addr;
  tmp_nd.family = family;
  tmp_nd.ret_val = ret;

  bpf_get_current_comm(&tmp_nd.com, TASK_COMM_LEN);
  bpf_perf_event_output(ctx, &test_map, BPF_F_CURRENT_CPU, &tmp_nd, sizeof(struct test_data));
}
#endif
/*  
 * below are traces point for syscall
*/

SEC("tracepoint/syscalls/sys_enter_connect")
// int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int tp_sys_enter_connect(struct trace_event_raw_sys_enter *ctx)
{

  uint64_t id = bpf_get_current_pid_tgid();
  struct conn_param_t conn_param = {};
  conn_param.fd = ctx->args[0];
  conn_param.addr = (struct sockaddr *)ctx->args[1];
  bpf_map_update_elem(&conn_param_map, &id, &conn_param, BPF_ANY);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, conn_param.fd, conn_param.addr, 0, 1);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_connect")
// int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int tp_sys_exit_connect(struct trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  const struct conn_param_t *conn_param = bpf_map_lookup_elem(&conn_param_map, &id); 
  if (conn_param != NULL) {
    trace_exit_connect(ctx, id, conn_param);
  }
  bpf_map_delete_elem(&conn_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, conn_param->fd, NULL, ctx->ret, 2);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_accept")
 // int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int tp_sys_enter_accept(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct accept_param_t accept_param = {};
  accept_param.addr = (struct sockaddr *)ctx->args[1];
  bpf_map_update_elem(&accept_param_map, &id, &accept_param, BPF_ANY);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], accept_param.addr, 0, 3);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_accept")
int tp_sys_exit_accept(struct trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct accept_param_t *accept_param = bpf_map_lookup_elem(&accept_param_map, &id);
  if (accept_param != NULL) {
    trace_exit_accept(ctx, id, accept_param);
  }
  bpf_map_delete_elem(&accept_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 4);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_accept4")
// int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
int tp_sys_enter_accept4(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct accept_param_t accept_param = {};
  accept_param.addr = (struct sockaddr *)ctx->args[1];
  bpf_map_update_elem(&accept_param_map, &id, &accept_param, BPF_ANY);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], (struct sockaddr *)ctx->args[1], 0, 5);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_accept4")
int tp_sys_exit_accept4(struct trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct accept_param_t *accept_param = bpf_map_lookup_elem(&accept_param_map, &id);
  if (accept_param != NULL) {
    trace_exit_accept(ctx, id, accept_param);
  }
  bpf_map_delete_elem(&accept_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 6);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_close")
// int close(int fd);
int tp_sys_enter_close(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();

  struct close_param_t close_param;
  close_param.fd = ctx->args[0];
  bpf_map_update_elem(&close_param_map, &id, &close_param, BPF_ANY);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], NULL, 0, 7);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_close")
int tp_sys_exit_close(struct trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  const struct close_param_t *close_param = bpf_map_lookup_elem(&close_param_map, &id);
  if (close_param != NULL) {
    trace_exit_close(ctx, id, close_param);
  }
  bpf_map_delete_elem(&close_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 8);
#endif
  return 0;
}

SEC("kretprobe/sock_alloc")
// struct socket *sock_alloc(void)
int BPF_KRETPROBE(kretprobe_sock_alloc)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct accept_param_t *accept_param = bpf_map_lookup_elem(&accept_param_map, &id);
  if (accept_param == NULL) {
    return 0;
  }
  if (accept_param->accept_socket == NULL) {
    accept_param->accept_socket = (struct socket *)PT_REGS_RC(ctx);
  }

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, 0, 10);
#endif
  return 0;
}

SEC("kprobe/security_socket_sendmsg")
int BPF_KPROBE(kprobe_security_socket_sendmsg)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct data_param_t *write_param = bpf_map_lookup_elem(&write_param_map, &id);
  if (write_param != NULL) {
    write_param->real_conn = true;
  }

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, 0, 11);
#endif
  return 0;
}

SEC("kprobe/security_socket_recvmsg")
// int security_socket_recvmsg(struct socket *sock, struct msghdr *msg, int size)
int BPF_KPROBE(kprobe_security_socket_recvmsg)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct data_param_t *read_param = bpf_map_lookup_elem(&read_param_map, &id);
  if (read_param != NULL) {
    read_param->real_conn = true;
  }

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, 0, 12);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_write")
// ssize_t write(int fd, const void *buf, size_t count);
int tp_sys_enter_write(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct data_param_t write_param = {};
  write_param.syscall_func = FuncWrite;
  write_param.fd = ctx->args[0];
  write_param.buf = (const char *)ctx->args[1];
  bpf_map_update_elem(&write_param_map, &id, &write_param, BPF_ANY);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], NULL, 0, 13);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_write")
int tp_sys_exit_write(struct trace_event_raw_sys_exit *ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t return_bytes = ctx->ret;

  struct data_param_t *write_param = bpf_map_lookup_elem(&write_param_map, &id);
  if (write_param != NULL && write_param->real_conn) {
    trace_exit_data(ctx, id, DirEgress, write_param, return_bytes, false);
  }
  bpf_map_delete_elem(&write_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 14);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_read")
// ssize_t read(int fd, void *buf, size_t count);
int tp_sys_enter_read(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct data_param_t read_param = {};
  read_param.syscall_func = FuncRead;
  read_param.fd = ctx->args[0];
  read_param.buf = (const char *)ctx->args[1];
  bpf_map_update_elem(&read_param_map, &id, &read_param, BPF_ANY);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], NULL, 0, 15);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_read")
int tp_sys_exit_read(struct trace_event_raw_sys_exit *ctx) {
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t return_bytes = ctx->ret;

  struct data_param_t *read_param = bpf_map_lookup_elem(&read_param_map, &id);
  if (read_param != NULL && read_param->real_conn) {
    trace_exit_data(ctx, id, DirIngress, read_param, return_bytes, false);
  }
  bpf_map_delete_elem(&read_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 16);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_sendto")
// ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
//                const struct sockaddr *dest_addr, socklen_t addrlen);
int tp_sys_enter_sendto(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct sockaddr addr;
  bpf_probe_read(&addr, sizeof(struct sockaddr), (struct sockaddr *)ctx->args[4]);
  struct sockaddr *paddr = &addr;

  if (paddr) {
    struct conn_param_t conn_param = {};
    conn_param.fd = ctx->args[0];
    conn_param.addr = (struct sockaddr *)ctx->args[4];
    bpf_map_update_elem(&conn_param_map, &id, &conn_param, BPF_ANY);
  }

  struct data_param_t write_param = {};
  write_param.syscall_func = FuncSendTo;
  write_param.fd = ctx->args[0];
  write_param.buf = (const char *)ctx->args[1];
  bpf_map_update_elem(&write_param_map, &id, &write_param, BPF_ANY);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], (const struct sockaddr *)ctx->args[4], 0, 17);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_sendto")
int tp_sys_exit_sendto(struct trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t return_bytes = ctx->ret;

  const struct conn_param_t *conn_param = bpf_map_lookup_elem(&conn_param_map, &id);
  if (conn_param != NULL && return_bytes > 0) {
    trace_reserve_conn(ctx, id, conn_param);
  }
  bpf_map_delete_elem(&conn_param_map, &id);

  struct data_param_t *write_param = bpf_map_lookup_elem(&write_param_map, &id);
  if (write_param != NULL) {
    trace_exit_data(ctx, id, DirEgress, write_param, return_bytes, false);
  }
  bpf_map_delete_elem(&write_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 18);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_recvfrom")
// ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
//                  struct sockaddr *src_addr, socklen_t *addrlen);
int tp_sys_enter_recvfrom(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct sockaddr addr;
  bpf_probe_read(&addr, sizeof(struct sockaddr), (struct sockaddr *)ctx->args[4]);
  struct sockaddr *paddr = &addr;

  if (paddr != NULL) {
    struct conn_param_t conn_param = {};
    conn_param.fd = ctx->args[0];
    conn_param.addr = (const struct sockaddr *)ctx->args[4];
    bpf_map_update_elem(&conn_param_map, &id, &conn_param, BPF_ANY);
  }

  struct data_param_t read_param = {};
  read_param.syscall_func = FuncRecvFrom;
  read_param.fd = ctx->args[0];
  read_param.buf = (const char *)ctx->args[1];
  bpf_map_update_elem(&read_param_map, &id, &read_param, BPF_ANY);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], (const struct sockaddr *)ctx->args[4], 0, 19);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_recvfrom")
int tp_sys_exit_recvfrom(struct  trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t return_bytes = ctx->ret;

  const struct conn_param_t *conn_param = bpf_map_lookup_elem(&conn_param_map, &id);
  if (conn_param != NULL && return_bytes > 0) {
    trace_reserve_conn(ctx, id, conn_param);
  }
  bpf_map_delete_elem(&conn_param_map, &id);

  struct data_param_t *read_param = bpf_map_lookup_elem(&read_param_map, &id);
  if (read_param != NULL) {
    trace_exit_data(ctx, id, DirIngress, read_param, return_bytes, false);
  }
  bpf_map_delete_elem(&read_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 20);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_sendmsg")
// ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
int tp_sys_enter_sendmsg(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct user_msghdr msghdr;
  bpf_probe_read(&msghdr, sizeof(struct user_msghdr), (struct user_msghdr *)ctx->args[1]);
  struct user_msghdr *pmsghdr = &msghdr;
  struct sockaddr msg_name;
  BPF_CORE_READ_INTO(&msg_name, pmsghdr, msg_name); 
  struct sockaddr *pmsg_name = &msg_name;

  if (pmsghdr != NULL) {
    if (pmsg_name != NULL) {
      struct conn_param_t conn_param = {};
      conn_param.fd = ctx->args[0];
      // conn_param.addr = msghdr->msg_name;
      BPF_CORE_READ_INTO(&conn_param.addr, pmsghdr, msg_name);
      bpf_map_update_elem(&conn_param_map, &id, &conn_param, BPF_ANY);
    }

    struct data_param_t write_param = {};
    write_param.syscall_func = FuncSendMsg;
    write_param.fd = ctx->args[0];
    // write_param.iov = msghdr->msg_iov;
    // write_param.iovlen = msghdr->msg_iovlen;
    BPF_CORE_READ_INTO(&write_param.iov, pmsghdr, msg_iov);
    BPF_CORE_READ_INTO(&write_param.iovlen, pmsghdr, msg_iovlen);
    bpf_map_update_elem(&write_param_map, &id, &write_param, BPF_ANY);
  }

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], NULL, 0, 21);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_exit_sendmsg")
int tp_sys_exit_sendmsg(struct trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t return_bytes = ctx->ret;

  const struct conn_param_t *conn_param = bpf_map_lookup_elem(&conn_param_map, &id);
  if (conn_param != NULL && return_bytes > 0) {
    trace_reserve_conn(ctx, id, conn_param);
  }
  bpf_map_delete_elem(&conn_param_map, &id);

  struct data_param_t *write_param = bpf_map_lookup_elem(&write_param_map, &id);
  if (write_param != NULL) {
    trace_exit_data(ctx, id, DirEgress, write_param, return_bytes, true);
  }
  bpf_map_delete_elem(&write_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 22);
#endif
  return 0;
}

SEC("tracepoint/syscalls/sys_enter_recvmsg")
// ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
int tp_sys_enter_recvmsg(struct trace_event_raw_sys_enter *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  struct user_msghdr msghdr;
  bpf_probe_read(&msghdr, sizeof(struct user_msghdr), (struct user_msghdr *)ctx->args[1]);
  struct user_msghdr *pmsghdr = &msghdr;
  struct sockaddr msg_name;
  BPF_CORE_READ_INTO(&msg_name, pmsghdr, msg_name); 
  struct sockaddr *pmsg_name = &msg_name;

  if (pmsghdr != NULL) {
    if (pmsg_name != NULL) {
      struct conn_param_t conn_param = {};
      conn_param.fd = ctx->args[0];
      // conn_param.addr = msghdr->msg_name;
      BPF_CORE_READ_INTO(&conn_param.addr, pmsghdr, msg_name);
      bpf_map_update_elem(&conn_param_map, &id, &conn_param, BPF_ANY);
    }

    struct data_param_t read_param = {};
    read_param.syscall_func = FuncRecvMsg;
    read_param.fd = ctx->args[0];
    // read_param.iov = msghdr->msg_iov;
    // read_param.iovlen = msghdr->msg_iovlen;
    BPF_CORE_READ_INTO(&read_param.iov, pmsghdr, msg_iov);
    BPF_CORE_READ_INTO(&read_param.iovlen, pmsghdr, msg_iovlen);
    bpf_map_update_elem(&read_param_map, &id, &read_param, BPF_ANY);
  }

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, ctx->args[0], NULL, 0, 23);
#endif
  return 0;
}

SEC("uprobe/ebpf_cleanup_dog")
int cleanup_dog_probe(struct pt_regs* ctx)
{
	int i;
	struct connect_info_t *conn_info;
	struct connect_id_t conn_id = {0};
	int conn_size = sizeof(struct connect_id_t);
	struct connect_id_t *conn_ids = (struct connect_id_t *)PT_REGS_PARM1(ctx);
	int size = PT_REGS_PARM2(ctx);
#pragma unroll
	for (i = 0; i < CONN_CLEANUP_NUMS; i++) {
		bpf_probe_read(&conn_id, conn_size, &conn_ids[i]);
#ifdef NET_TEST
		net_bpf_print("dog probe used, tgpid:%u, fd:%d\n", conn_id.tgid, conn_id.fd);
#endif
		if (i >= size) {
			break;
		}
		uint64_t tgid_fd = combine_tgid_fd(conn_id.tgid, conn_id.fd);
		conn_info = (struct connect_info_t *)bpf_map_lookup_elem(&connect_info_map, &tgid_fd);
		if (conn_info != NULL && conn_info->conn_id.tgid == conn_id.tgid) {
			bpf_map_delete_elem(&connect_info_map, &tgid_fd);
		}
	}
}

SEC("tracepoint/syscalls/sys_exit_recvmsg")
int tp_sys_exit_recvmsg(struct trace_event_raw_sys_exit *ctx)
{
  uint64_t id = bpf_get_current_pid_tgid();
  ssize_t return_bytes = ctx->ret;

  const struct conn_param_t *conn_param = bpf_map_lookup_elem(&conn_param_map, &id);
  if (conn_param != NULL && return_bytes > 0) {
    trace_reserve_conn(ctx, id, conn_param);
  }
  bpf_map_delete_elem(&conn_param_map, &id);

  struct data_param_t *read_param = bpf_map_lookup_elem(&read_param_map, &id);
  if (read_param != NULL) {
    trace_exit_data(ctx, id, DirIngress, read_param, return_bytes, true);
  }

  bpf_map_delete_elem(&read_param_map, &id);

#ifdef NET_TEST
  test_bpf_syscall(ctx, id, 0, NULL, ctx->ret, 24);
#endif
  return 0;
}

#if 0
static __always_inline
bool trace_allowed(u32 tgid, u32 pid)
{
	u32 uid;

	/* filters */
	if (targ_tgid && targ_tgid != tgid)
		return false;
	if (targ_pid && targ_pid != pid)
		return false;
	if (valid_uid(targ_uid)) {
		uid = (u32)bpf_get_current_uid_gid();
		if (targ_uid != uid) {
			return false;
		}
	}
	return true;
}

SEC("tracepoint/syscalls/sys_enter_open")
int tracepoint__syscalls__sys_enter_open(struct trace_event_raw_sys_enter* ctx)
{
	u64 id = bpf_get_current_pid_tgid();
	/* use kernel terminology here for tgid/pid: */
	u32 tgid = id >> 32;
	u32 pid = id;

	/* store arg info for later lookup */
	if (trace_allowed(tgid, pid)) {
		struct args_t args = {};
		args.fname = (const char *)ctx->args[0];
		args.flags = (int)ctx->args[1];
		bpf_map_update_elem(&start, &pid, &args, 0);
	}
	return 0;
}
static __always_inline
int trace_exit(struct trace_event_raw_sys_exit* ctx)
{
	struct event event = {};
	struct args_t *ap;
	int ret;
	u32 pid = bpf_get_current_pid_tgid();

	ap = bpf_map_lookup_elem(&start, &pid);
	if (!ap)
		return 0;	/* missed entry */
	ret = ctx->ret;
	if (targ_failed && ret >= 0)
		goto cleanup;	/* want failed only */

	/* event data */
	event.pid = bpf_get_current_pid_tgid() >> 32;
	event.uid = bpf_get_current_uid_gid();
	bpf_get_current_comm(&event.comm, sizeof(event.comm));
	bpf_probe_read_user_str(&event.fname, sizeof(event.fname), ap->fname);
	event.flags = ap->flags;
	event.ret = ret;

	/* emit event */
	bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU,
			      &event, sizeof(event));

cleanup:
	bpf_map_delete_elem(&start, &pid);
	return 0;
}

SEC("tracepoint/syscalls/sys_exit_open")
int tracepoint__syscalls__sys_exit_open(struct trace_event_raw_sys_exit* ctx)
{
	return trace_exit(ctx);
}
#endif

char LICENSE[] SEC("license") = "GPL";
