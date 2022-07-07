#include <stdlib.h>
#include <linux/types.h>
#include <time.h>

#define SEC_TO_NS	(1000*1000*1000)
void stamp_to_date(__u64 stamp, char dt[], int len)
{
	time_t t, diff, last;
	struct tm *tm;
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	last = time(&t);
	if (stamp) {
		diff = ts.tv_sec*SEC_TO_NS + ts.tv_nsec - stamp;
		diff = diff/SEC_TO_NS;
		last = t - diff;
	}
	tm = localtime(&last);
	strftime(dt, len, "%F %H:%M:%S", tm);
}
