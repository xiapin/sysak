#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <string>

using namespace std;

#define PAGESIZE 4096
#define MALLOC_SIZE (20 * 1024 * 1024)

void* foo(void* arg)
{
	while(1) {
		char *p=(char*)malloc(MALLOC_SIZE);
		int i; 

		if (!p)
			exit(1);
		for(i = 0; i < MALLOC_SIZE/PAGESIZE; i++) {
			*p = 'a';
			p += PAGESIZE;
		}
	}

	return NULL;
}

int main(int argc,char** argv)
{
	int i, thread_nr, cpu_nr;
	pthread_t thread;

	if(pthread_create(&thread, NULL, foo, NULL))
		return -1;

	pthread_join(thread,NULL);
	return 0;
}
