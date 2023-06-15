
#define DEBUG	1

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

int cpuid_L3_RMID_eventID(void)
{
	/*
	 * Factor: CPUID.(EAX=0FH, ECX=1H).EBX
	 * RMID:   CPUID.(EAX=0FH, ECX=1H).ECX
	 * EVENTID:CPUID.(EAX=0FH, ECX=1H).EDX
	 * */
	int EBX, ECX, EDX;

	__asm__("mov $0xF, %eax\n\t");
	__asm__("mov $0x1, %ecx\n\t");	/* 0x1 is from cpuid_L3_type_supported() */
	__asm__("cpuid\n\t");
	__asm__("mov %%ebx, %0\n\t":"=r" (EBX));
	__asm__("mov %%ecx, %0\n\t":"=r" (ECX));
	__asm__("mov %%edx, %0\n\t":"=r" (EDX));
#ifdef  DEBUG
	printf ("factor=%d, RMID=%lu, eventid==0x%x\n", EBX, ECX, EDX);
#endif
}
