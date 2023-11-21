#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "iosdiag.h"
#include "format_json.h"


int req_array_length = 0;
int req_capacity = 10;
struct iosdiag_req* req_array;

void reset_req_statistics() 
{
	req_array_length = 0;
	req_capacity = 10;
	req_array = (struct iosdiag_req*)realloc(req_array, req_capacity * sizeof(struct iosdiag_req));
}

void expand_req_array() 
{
	req_capacity *= 2;
	req_array = (struct iosdiag_req*)realloc(req_array, req_capacity * sizeof(struct iosdiag_req));
}

/* Update various statistical variables */
void update_component_delay(struct iosdiag_req *iop, unsigned long *sum_delay, unsigned long *max_delay, int status)
{
    int m = 1;
    unsigned long delay = 0;
    for (; m < MAX_POINT; m++) {
    	if (iop->ts[m] > iop->ts[m-1]) {
    		delay = iop->ts[m] / 1000 - iop->ts[m-1] / 1000;
			sum_delay[m-1] += delay;
            if (status) {
                max_delay[m-1] = delay;
            }
    	}
    }
}

void init_aggregation_metrics(struct aggregation_metrics* ams, struct iosdiag_req *iop, int max_index)
{
	/* Initialize some statistical variables */
	ams->sum_data_len = iop->data_len;
    ams->sum_max_delay = get_max_delay(iop);
	ams->max_delay = ams->sum_max_delay;
    ams->sum_total_delay = get_total_delay(iop);
	ams->max_total_delay = ams->sum_total_delay;
	ams->max_total_dalay_idx = max_index;
    ams->maxdelay_component = get_max_delay_component(iop);
	ams->max_total_delay_diskname = iop->diskname;
    ams->sum_component_delay = 
		(unsigned long*)malloc(5 * sizeof(unsigned long));
	memset(ams->sum_component_delay, 0, 5 * sizeof(unsigned long));
    ams->max_component_delay = 
		(unsigned long*)malloc(5 * sizeof(unsigned long));
    memset(ams->max_component_delay, 0, 5 * sizeof(unsigned long));
	ams->count = 1;
	update_component_delay(iop, ams->sum_component_delay, ams->max_component_delay, 1);
}

int check_aggregation_conditions(struct iosdiag_req *iop1, struct iosdiag_req *iop2)
{
    int res = 0;
    if (strcmp(iop1->diskname, iop2->diskname) == 0 && 
	strcmp(iop1->comm, iop2->comm) == 0 && 
	strcmp(iop1->op, iop2->op) == 0 && 
	strcmp(get_max_delay_component(iop1), get_max_delay_component(iop2)) == 0 && 
	// iop1->cpu[0] == iop2->cpu[0] && 
	// iop1->cpu[1] == iop2->cpu[1] && 
	// iop1->cpu[2] == iop2->cpu[2] && 
	// iop1->cpu[3] == iop2->cpu[3]  && 
	iop1->queue_id == iop2->queue_id) {
        res = 1;
    }
    return res;
}

int check_aggregated(struct iosdiag_req *iop)
{
	if (strlen(iop->diskname) == 0 || strlen(iop->comm) == 0 || strlen(iop->op) == 0) {
    	return 1; 
    }
	return 0;
}

/* Update various statistical variables */
void aggregate_events(struct aggregation_metrics* ams, struct iosdiag_req *iop, int new_index)
{
    ams->count ++;
	unsigned long aggregated_total_delay = get_total_delay(iop);
	unsigned long aggregated_max_delay = get_max_delay(iop);
    ams->sum_total_delay += aggregated_total_delay;
	ams->sum_max_delay += aggregated_max_delay;
    ams->sum_data_len += iop->data_len;
	// printf("max_total2 %d: %lu\n", new_index, aggregated_total_delay);

	if (ams->max_total_delay < aggregated_total_delay) {
		ams->max_delay = aggregated_max_delay;
		ams->max_total_delay = aggregated_total_delay;
		ams->max_total_dalay_idx = new_index;
		update_component_delay(iop, ams->sum_component_delay, ams->max_component_delay, 1);
	} else {
		update_component_delay(iop, ams->sum_component_delay, ams->max_component_delay, 0);
	}
	//printf("put %d into blank\n", j);
	memset(iop->diskname, '\0', sizeof(iop->diskname));
}

/* Update various statistical variables */
void post_aggregation_statistics(struct aggregation_metrics* ams)
{
	int m = 0;
    // ams->sum_data_len /= ams->count;
    ams->sum_max_delay /= ams->count;
    ams->sum_total_delay /= ams->count;
	for (; m < MAX_POINT - 1; m++) {
    	ams->sum_component_delay[m] /= ams->count;
		// printf("sum_component_delay: %d, %lu\n", ams->count, ams->sum_component_delay[m]);
	}
}

void aggregation_summary_convert_to_unity(char* dest, struct iosdiag_req *iop, struct iosdiag_req *iop_max, struct aggregation_metrics* ams)
{
	sprintf(dest, 
		"sysom_iolatency,diskname=%s,"
		"comm=%s,"
		"iotype=%s,"
		"pid=%d,"
		"ppid=%d,"
		"queue_id=%d,"
		"maxdelay_component=%s "
		"max_delay=%lu,"
		"total_delay=%lu,"
		"sum_datalen=%d,"
		"datalen=%d,"
		"initiated_cpu=%d,"
		"issue_cpu=%d,"
		"respond_cpu=%d,"
		"soft_interrupt_cpu=%d,"
		"block=%lu,"
		"driver=%lu,"
		"disk=%lu,"
		"complete=%lu,"
		"done=%lu,"
		"count=%d",
		iop->diskname, 
		iop->comm, 
		iop->op, 
		iop->tid, 
		iop->pid, 
		iop->queue_id,
		ams->maxdelay_component, 
		ams->sum_max_delay, 
		ams->sum_total_delay, 
		ams->sum_data_len,
		ams->sum_data_len/ams->count,
		iop->cpu[0],
		iop->cpu[1],
		iop->cpu[2],
		iop->cpu[3],
		ams->sum_component_delay[0],
		ams->sum_component_delay[1],
		ams->sum_component_delay[2],
		ams->sum_component_delay[3],
		ams->sum_component_delay[4],
		ams->count
	);
	sprintf(dest + strlen(dest), "%s", "\n");

	sprintf(dest + strlen(dest), 
		"sysom_iolatency_max,diskname=%s,"
		"comm=%s,"
		"iotype=%s,"
		"pid=%d,"
		"ppid=%d,"
		"queue_id=%d,"
		"maxdelay_component=%s "
		"max_delay=%lu,"
		"total_delay=%lu,"
		"sum_datalen=%d,"
		"datalen=%d,"
		"initiated_cpu=%d,"
		"issue_cpu=%d,"
		"respond_cpu=%d,"
		"soft_interrupt_cpu=%d,"
		"block=%lu,"
		"driver=%lu,"
		"disk=%lu,"
		"complete=%lu,"
		"done=%lu,"
		"count=%d",
		ams->max_total_delay_diskname, 
		iop_max->comm, 
		iop_max->op, 
		iop_max->tid,
		iop_max->pid,
		iop_max->queue_id,
		ams->maxdelay_component, 
		ams->max_delay, 
		ams->max_total_delay, 
		ams->sum_data_len, 
		iop_max->data_len,
		iop_max->cpu[0],
		iop_max->cpu[1],
		iop_max->cpu[2],
		iop_max->cpu[3],
		ams->max_component_delay[0],
		ams->max_component_delay[1],
		ams->max_component_delay[2],
		ams->max_component_delay[3],
		ams->max_component_delay[4],
		ams->count
	);
}




