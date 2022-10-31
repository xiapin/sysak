#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <linux/cgroupstats.h>
#include "stat_nl.h"
#include "getdelays.h"

int DEBUG;
static int send_cmd(int sd, __u16 nlmsg_type, __u32 nlmsg_pid,
	     __u8 genl_cmd, __u16 nla_type,
	     void *nla_data, int nla_len)
{
	struct nlattr *na;
	struct sockaddr_nl nladdr;
	int r, buflen;
	char *buf;

	struct msgtemplate msg;

	msg.n.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
	msg.n.nlmsg_type = nlmsg_type;
	msg.n.nlmsg_flags = NLM_F_REQUEST;
	msg.n.nlmsg_seq = 0;
	msg.n.nlmsg_pid = nlmsg_pid;
	msg.g.cmd = genl_cmd;
	msg.g.version = 0x1;
	na = (struct nlattr *) GENLMSG_DATA(&msg);
	na->nla_type = nla_type;
	na->nla_len = nla_len + NLA_HDRLEN;
	memcpy(NLA_DATA(na), nla_data, nla_len);
	msg.n.nlmsg_len += NLMSG_ALIGN(na->nla_len);

	buf = (char *) &msg;
	buflen = msg.n.nlmsg_len ;
	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;
	while ((r = sendto(sd, buf, buflen, 0, (struct sockaddr *) &nladdr,
			   sizeof(nladdr))) < buflen) {
		if (r > 0) {
			buf += r;
			buflen -= r;
		} else if (errno != EAGAIN)
			return -1;
	}
	return 0;
}

#define average_ms(t, c) (t / 1000000ULL / (c ? c : 1))

void print_delayacct_sum(FILE *out, struct taskstats *t1, struct taskstats *t2, 
			struct irq_acct *irqst, struct syscalls_acct *sysacct)
{
	struct taskstats t;

	t.cpu_count = t2->cpu_count - t1->cpu_count;
	t.cpu_delay_total = t2->cpu_delay_total - t1->cpu_delay_total;
	t.blkio_delay_total = t2->blkio_delay_total - t1->blkio_delay_total;
	t.blkio_count = t2->blkio_count - t1->blkio_count;
	t.swapin_count = t2->swapin_count - t1->swapin_count;
	t.swapin_delay_total = t2->swapin_delay_total - t1->swapin_delay_total;
	t.freepages_count = t2->freepages_count - t1->freepages_count;
	t.freepages_delay_total = t2->freepages_delay_total - t1->freepages_delay_total;

	fprintf(out, "%-12s%-15s%-15s%-15s%-15s(ms)\n", "WHAT", "COUNT", "TIME", "AVG", "SYS");
	fprintf(out, "%-12s%-15llu%-15.3f%-15.3f%-15s\n", "CPU", 
		(unsigned long long)t.cpu_count,
		(double)t.cpu_delay_total/1000000,
		average_ms((double)t.cpu_delay_total, t.cpu_count),
		"N/A");
	fprintf(out, "%-12s%-15llu%-15.3f%-15.3f%-15s\n", "IO", 
	       (unsigned long long)t.blkio_count,
	       (double)t.blkio_delay_total/1000000,
	       average_ms((double)t.blkio_delay_total, t.blkio_count),
		"N/A");
	fprintf(out, "%-12s%-15llu%-15.3f%-15.3f%-15s\n", "SWAP", 
	       (unsigned long long)t.swapin_count,
	       (double)t.swapin_delay_total/1000000,
	       average_ms((double)t.swapin_delay_total, t.swapin_count),
		"N/A");
	fprintf(out, "%-12s%-15llu%-15.3f%-15.3f%-15s\n", "RECLAIM", 
	       (unsigned long long)t.freepages_count,
	       (double)t.freepages_delay_total/1000000,
	       average_ms((double)t.freepages_delay_total, t.freepages_count),
		"N/A");
	fprintf(out, "%-12s%-15llu%-15.3f%-15.3f%-15s\n", "IRQ", 
	       (unsigned long long)irqst->cnt,
	       (double)irqst->delay/1000000,
	       average_ms((double)irqst->delay, irqst->cnt),
		"N/A");
	fprintf(out, "%-12s%-15llu%-15.3f%-15.3f%-15llu\n", "SYSCALL", 
		(unsigned long long)sysacct->cnt,
		(double)sysacct->delay/1000000,
		average_ms((double)sysacct->delay, sysacct->cnt),
		sysacct->sys/1000000);
}

int get_nl_stats(int cmd_type, pid_t pid, struct msgtemplate *msg, struct taskstats **tstats)
{
	pid_t rtid = 0;
	__u16 id;
	char name[64];
	__u32 mypid;
	struct nlattr *na;
	int loop = 0, len = 0;
	struct sockaddr_nl local;
	int rc, nl_fd, rep_len, aggr_len, len2;

	nl_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if (nl_fd < 0) {
		rc = errno;
		fprintf(stderr,"socket:%s", strerror(errno));
		return rc;
	}
	memset(&local, 0, sizeof(local));
	local.nl_family = AF_NETLINK;

	if (bind(nl_fd, (struct sockaddr *) &local, sizeof(local)) < 0) {
		rc = errno;
		fprintf(stderr,"bind:%s", strerror(errno));
		close(nl_fd);
		return rc;
	}
	strcpy(name, TASKSTATS_GENL_NAME);
	mypid = getpid();
	rc = send_cmd(nl_fd, GENL_ID_CTRL, mypid, CTRL_CMD_GETFAMILY,
		CTRL_ATTR_FAMILY_NAME, (void *)name,
		strlen(TASKSTATS_GENL_NAME)+1);
	if (rc < 0) {
		rc = errno;
		fprintf(stderr,"send_cmd1:%s", strerror(errno));
		close(nl_fd);
		return rc;
	}
	rep_len = recv(nl_fd, msg, sizeof(struct msgtemplate), 0);
	if (msg->n.nlmsg_type == NLMSG_ERROR ||
	    (rep_len < 0) || !NLMSG_OK((&msg->n), rep_len)) {
		fprintf(stderr,"recv FAMILY_ID:%s", strerror(errno));
		close(nl_fd);
		return -1;
	}
	na = (struct nlattr *) GENLMSG_DATA(msg);
	na = (struct nlattr *) ((char *) na + NLA_ALIGN(na->nla_len));
	id = 0;
	if (na->nla_type == CTRL_ATTR_FAMILY_ID) {
		 id = *(__u16 *) NLA_DATA(na);
	}
	if (!id) {
		fprintf(stderr,"bad family ID\n");
		close(nl_fd);
		return -1;
	}
	if (pid) {
		rc = send_cmd(nl_fd, id, mypid, TASKSTATS_CMD_GET,
			cmd_type, &pid, sizeof(__u32));
		if (rc < 0) {
			rc = errno;
			fprintf(stderr,"send_cmd2:%s", strerror(errno));
			close(nl_fd);
			return rc;
		}
	} else {
		close(nl_fd);
		return -1;
	}

	memset(msg, 0, sizeof(struct msgtemplate));
	do {
		rep_len = recv(nl_fd, msg, sizeof(struct msgtemplate), 0);
		if (rep_len < 0) {
			rep_len = errno;
			fprintf(stderr,"recv2.0:%s", strerror(errno));
			close(nl_fd);
			return rep_len;
		}
		if (msg->n.nlmsg_type == NLMSG_ERROR ||
			!NLMSG_OK((&msg->n), rep_len)) {
			struct nlmsgerr *err = NLMSG_DATA(&msg);
			fprintf(stderr,"recv2.1:%s", strerror(err->error));
			close(nl_fd);
			return err->error;
		}

		rep_len = GENLMSG_PAYLOAD(&msg->n);
		na = (struct nlattr *) GENLMSG_DATA(msg);
		len = 0;
		while (len < rep_len) {
			len += NLA_ALIGN(na->nla_len);
			switch (na->nla_type) {
			case TASKSTATS_TYPE_AGGR_TGID:
			case TASKSTATS_TYPE_AGGR_PID:
				aggr_len = NLA_PAYLOAD(na->nla_len);
				len2 = 0;
				na = (struct nlattr *) NLA_DATA(na);
				while (len2 < aggr_len) {
					switch (na->nla_type) {
					case TASKSTATS_TYPE_PID:
						if (DEBUG) {
							rtid = *(int *) NLA_DATA(na);
							printf("PID\t%d\n", rtid);
						}
						break;
					case TASKSTATS_TYPE_TGID:
						if (DEBUG) {
							rtid = *(int *) NLA_DATA(na);
							printf("TGID\t%d\n", rtid);
						}
						break;
					case TASKSTATS_TYPE_STATS:
						*tstats = ((struct taskstats *) NLA_DATA(na));
						if (nl_fd) {
							if (write(nl_fd, NLA_DATA(na), na->nla_len) < 0) {
								fprintf(stderr,"write error\n");
							}
						}
						if (!loop)
							goto done;
						break;
					case TASKSTATS_TYPE_NULL:
						break;
					default:
						fprintf(stderr, "Unknown nested"
							" nla_type %d\n",
							na->nla_type);
						break;	
					}
					len2 += NLA_ALIGN(na->nla_len);
					na = (struct nlattr *)((char *)na +
							       NLA_ALIGN(na->nla_len));
				}
				break;
			default:
				fprintf(stderr, "Unknown nla_type %d\n",
					na->nla_type);
			case TASKSTATS_TYPE_NULL:
				break;
			}
			na = (struct nlattr *) (GENLMSG_DATA(msg) + len);
		}
	} while (loop);
done:
	close(nl_fd);
	return 0;
}

#ifdef SELF_TEST
int main(int argc, char *argv[])
{
	int ret;
	pid_t pid;
	struct msgtemplate msg;
	struct taskstats *tstats;

	if (argc != 2) {
		printf("usage: %s pid\n", argv[0]);
		return -1;
	}
	pid = strtol(argv[1], NULL, 10);
	if (errno || pid <= 0) {
		fprintf(stderr, "Invalid PID: %s\n", argv[1]);
		return errno;
	}
	
	ret = get_nl_stats(pid, &msg, tstats);
	if (!ret) {
		print_delayacct_sum(*tstats);
	}
}
#endif
