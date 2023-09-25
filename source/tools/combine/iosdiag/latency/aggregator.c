#include <sys/types.h>
#include <unistd.h>
#include "iosdiag.h"
#include "aggregator.h"
#include "format_json.h"

void update_component_delay(struct iosdiag_req *iop, unsigned long *sum_delay, unsigned long *max_delay, int status)
{
    int m, n = 0;
    int skip = 0;
    unsigned long delay = 0;
    char *component;
    for (m = 0, n = 0; m < MAX_POINT; m++) {
    	if (m == IO_START_POINT) {
    		continue;
    	} else {
    		if (!skip)
    			n = m - 1;
    		if (iop->ts[m] > iop->ts[n]) {
    			delay = iop->ts[m] / 1000 - iop->ts[n] / 1000;
    			skip = 0;
				component = delay_idx_to_str(m);
				// printf("sum_max_delay: %lu\n", delay);
				if (strcmp(component, "block") == 0) {
    	  			sum_delay[0] += delay;
                    if (status) {
                        max_delay[0] = delay;
                    }
    			}else if (strcmp(component, "driver") == 0) {
    			 	sum_delay[1] += delay;
                    if (status) {
                        max_delay[1] = delay;
                    }
    			}else if (strcmp(component, "disk") == 0) {
    			  	sum_delay[2] += delay;
                    if (status) {
                        max_delay[2] = delay;
                    }
    			}else if (strcmp(component, "complete") == 0) {
    			  	sum_delay[3] += delay;
                    if (status) {
                        max_delay[3] = delay;
                    }
    			}else if (strcmp(component, "done") == 0) {
    			  	sum_delay[4] += delay;
                    if (status) {
                        max_delay[4] = delay;
                    }
    			}
    		} else {
    			skip = 1;
    			continue;
    		}
    	}
    }
}

int check_aggregation_conditions(struct iosdiag_req *iop1, struct iosdiag_req *iop2)
{
    int res = 0;
    if (strcmp(iop1->diskname, iop2->diskname) == 0 && 
	strcmp(iop1->comm, iop2->comm) == 0 && 
	strcmp(iop1->op, iop2->op) == 0 && 
	strcmp(get_max_delay_component(iop1), get_max_delay_component(iop2)) == 0 && 
	iop1->cpu[0] == iop2->cpu[0] && 
	iop1->cpu[1] == iop2->cpu[1] && 
	iop1->cpu[2] == iop2->cpu[2] && 
	iop1->cpu[3] == iop2->cpu[3]  && 
	iop1->queue_id == iop2->queue_id) {
        res = 1;
    }
    return res;
}