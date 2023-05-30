#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

void create_process(int n) {
    int pid[n];
    int i;
    for (i = 0; i < n; i++) {
        if ((pid[i] = fork()) == 0) {
            sleep(120);
            exit(0);
        }
    }

    for (i = 0; i < n; i++) {
        waitpid(pid[i], 0, 0);
    }
}

void *run_forever(void *arg) {
    while (1) {
    }
}

void run_multithread() {
    pthread_t pid[128];
    int i;
    for (i = 0; i < 128; i++) {
        pthread_create(&pid[i], 0, run_forever, 0);
        // printf("fork.\n");
    }

    for (i = 0; i < 128; i++) {
        pthread_join(pid[i], 0);
    }
}

void loop_fork() {
#define pnum 128
    while (1) {
        int i;
        int pid[pnum];
        for (i = 0; i < pnum; i++) {
            if ((pid[i] = fork()) == 0) {
                // usleep(1000);
                int j = 0;
                int a = 0;
                for (j = 0; j < 100000; j++) {
                    a++;
                }

                exit(0);
            }
        }

        for (i = 0; i < pnum; i++) {
            waitpid(pid[i], 0, 0);
        }
        usleep(1000);
    }
}

void *do_nothing(void *arg) { return 0; }

void loop_clone() {
    while (1) {
        pthread_t pid[128];
        int i;
        for (i = 0; i < 128; i++) {
            pthread_create(&pid[i], 0, do_nothing, 0);
            // printf("fork.\n");
        }

        for (i = 0; i < 128; i++) {
            pthread_join(pid[i], 0);
        }
    }
}

void cpu_bind(int cpu_id) {
    cpu_set_t set;
    int i;
    int cpu_num = -1;
    CPU_ZERO(&set);

    int pids[64];
    for (i = 0; i < 64; i++) {
        switch (pids[i] = fork()) {
            case -1: { /* Error */
                fprintf(stderr, "fork error\n");
                exit(EXIT_FAILURE);
            }
            case 0: { /* Child */
                CPU_SET(cpu_id, &set);
                if (sched_setaffinity(getpid(), sizeof(set), &set) == -1) {
                    fprintf(stderr, "child sched_setaffinity error\n");
                    exit(EXIT_FAILURE);
                }
                sleep(1);
                if (-1 != (cpu_num = sched_getcpu())) {
                    fprintf(stdout, "The child process is running on cpu %d\n",
                            cpu_num);
                }

                int a = 0;
                while (1) {
                    a = a << 1;
                }

                exit(EXIT_SUCCESS);
            }
        }
    }

    for (i = 0; i < 64; i++) {
        waitpid(pids[i], 0, 0);
    }
}

int main(int argc, char **argv) {
    if (argc > 3) {
        printf("usage: test [bind|fork|clone|multi_thread]\n");
        return -1;
    }

    if (!strcmp(argv[1], "clone")) {
        loop_clone();
    } else if (!strcmp(argv[1], "fork")) {
        loop_fork();
    } else if (!strcmp(argv[1], "bind")) {
        cpu_bind(0);
    } else if (!strcmp(argv[1], "multi_thread")) {
        sleep(10);
        run_multithread();
    } else if (!strcmp(argv[1], "sleep")) {
        create_process(atoi(argv[2]));
    }
}