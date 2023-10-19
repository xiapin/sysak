extern int req_array_length;
extern int req_capacity;
extern struct iosdiag_req* req_array;
pthread_mutex_t req_mutex;

void reset_req_statistics();
void expand_req_array();
void init_aggregation_metrics(struct aggregation_metrics* ams, struct iosdiag_req *iop, int max_index);
void update_component_delay(struct iosdiag_req *iop, unsigned long *sum_delay, unsigned long *max_delay, int status);
int check_aggregation_conditions(struct iosdiag_req *iop1, struct iosdiag_req *iop2);
int check_aggregated(struct iosdiag_req *iop);
void aggregate_events(struct aggregation_metrics* ams, struct iosdiag_req *iop, int new_index);
void aggregation_summary_convert_to_unity(char* dest, struct iosdiag_req *iop, struct iosdiag_req *iop_max, struct aggregation_metrics* ams);
void post_aggregation_statistics(struct aggregation_metrics* ams);




