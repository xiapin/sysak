#ifndef _FORMAT_JSON_H
#define _FORMAT_JSON_H

#define JSON_BUFFER_SIZE	4096
void set_check_time_date(void);
void summary_convert_to_json(void *dest, void *src);
void summary_convert_to_unity(void *dest, void *src);
void delay_convert_to_json(void *dest, void *src);
void point_convert_to_json(void *dest, void *src);
char *delay_idx_to_str(int idx);
unsigned long get_total_delay(struct iosdiag_req *iop);
unsigned long get_max_delay(struct iosdiag_req *iop);
char *get_max_delay_component(struct iosdiag_req *iop);
#endif

