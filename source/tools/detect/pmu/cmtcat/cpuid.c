#include <stdio.h>
#include <unistd.h>
#include <linux/types.h>
#include "msr.h"
#include "cpuid.h"
/**
 * check:  CPUID.(EAX=07H, ECX=0):EBX.PQM[bit 12] reports 1
 * 
 */

int main()
{
	int ret;

	if (!get_cpuid_maxleaf())
		return 0;
	if (!cpid_PQM_supported())
		return 0;
	if (!cpuid_L3_type_supported())
		return 0;
	cpuid_L3_RMID_eventID();
}

