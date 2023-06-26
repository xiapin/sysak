#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <string>

extern "C" int gettid(void);

using namespace std;

#define MAX_THREAD_NUM 300

static pthread_mutex_t foo_mutex;

void* foo(void* arg)
{
    char buff[128];
    int idx = (long)arg + 1;

    printf("thread start..\n");
//  sprintf(buff, "echo %d > /sys/fs/cgroup/cpu/test%i/tasks", gettid(), idx);
//  system(buff);

    while(1){
       pthread_mutex_lock(&foo_mutex);
       int size=rand()/(1024*150);
       char *p=(char*)malloc(size);
       memset(p,0x00,size);
       string xx;
       xx.assign(p,size);
       pthread_mutex_unlock(&foo_mutex);
       free(p);
    }

    printf("thread end..\n");

    return NULL;
}

int main(int argc,char** argv)
{
    pthread_mutex_init(&foo_mutex, NULL);

    pthread_t thread[MAX_THREAD_NUM];
    for(int i=0;i<MAX_THREAD_NUM;i++){
        if(pthread_create(&thread[i],NULL,foo,(void *)i))
            exit(1);
   }

   for(int i=0;i<MAX_THREAD_NUM;i++) {
      pthread_join(thread[i],NULL);
   }

   return 0;
}
