typedef union cpuid_info {
	int array[4];
	struct {
		unsigned int eax, ebx, ecx, edx;
	} reg;
	struct {
		unsigned int reserve, factor, rmid, eventid;
	} info;
} cpuid_info;

int get_cpuid_maxleaf(void)
{
        int maxleaf;
        __asm__("mov $0x0, %eax\n\t");
        __asm__("cpuid\n\t");
        __asm__("mov %%eax, %0\n\t":"=r" (maxleaf));
#ifdef	DEBUG
        printf ("maxleaf=0x%x\n", maxleaf);
#endif
        if (maxleaf > 7) {
                printf("maxleaf supported\n");
		return 1;
	}
        else {
                printf("maxleaf NOT supported\n");
		return 0;
	}	
}

int cpid_PQM_supported(void)
{
        int EBX;
        __asm__("mov $0x7, %eax\n\t");
        __asm__("mov $0x0, %ecx\n\t");
        __asm__("cpuid\n\t");
        __asm__("mov %%ebx, %0\n\t":"=r" (EBX));
#ifdef	DEBUG
        printf ("PQM=0x%x\n", EBX);
#endif
	if (EBX & (1 << 12)) {	/*EBX.PQM[bit 12]*/
		printf("PQM supported\n");
		return 1;
	}
	else {
		printf(":PQM NOT supported\n");
		return 0;
	}
}

int cpuid_L3_type_supported(void)
{
	/* check :EDX.L3[bit1] */
	int EDX, EBX;

	__asm__("mov $0xF, %eax\n\t");
	__asm__("mov $0x0, %ecx\n\t");
	__asm__("cpuid\n\t");
	__asm__("mov %%ebx, %0\n\t":"=r" (EBX));
	__asm__("mov %%edx, %0\n\t":"=r" (EDX));
#ifdef  DEBUG
	printf ("type=0x%x, rmid=%lu\n", EDX, EBX);
#endif
	if (EDX & (1 << 1)) {
		printf("L3_type supported\n");
		return 1;
	} else {
		printf("L3_type NOT supported\n");
		return 0;
	}
}

void cpuid(const unsigned leaf, const unsigned subleaf, cpuid_info* info)
{
	__asm__ __volatile__("cpuid"
			    :"=a"(info->reg.eax),
				"=b"(info->reg.ebx),
				"=c"(info->reg.ecx),
				"=d"(info->reg.edx)
			    :"a"(leaf), "c"(subleaf));
}

void cpuid_Factor_RMID_eventID(cpuid_info *cpuid_i)
{
	/*
 	 *  Factor: CPUID.(EAX=0FH, ECX=1H).EBX
 	 *  RMID:   CPUID.(EAX=0FH, ECX=1H).ECX
 	 *  EVENTID:CPUID.(EAX=0FH, ECX=1H).EDX
 	**/
	cpuid(0xF, 0x1, cpuid_i);
#ifdef  DEBUG
	printf("eventid=%lld\n", cpuid_i->info.eventid);
#endif
}

bool check_cpuid_support(void)
{
	if (!get_cpuid_maxleaf())
		return false;
	if (!cpid_PQM_supported())
		return false;
	return cpuid_L3_type_supported();
}
