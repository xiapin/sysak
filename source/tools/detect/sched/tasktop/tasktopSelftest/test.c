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
        }
    }

    for (i = 0; i < n; i++) {
        waitpid(pid[i], 0, 0);
    }
}

void loop_fork() {
    while (1) {
        int i;
        int pid[128];
        for (i = 0; i < 128; i++) {
            if ((pid[i] = fork()) == 0) {
                int a = 0;
                int b = 1;
                int c = a + b;
                exit(0);
            }
            // printf("fork.\n");
        }

        for (i = 0; i < 128; i++) {
            waitpid(pid[i], 0, 0);
        }
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

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: test clone or test fork\n");
        return -1;
    }

    if (!strcmp(argv[1], "clone")) {
        loop_clone();
    } else if (!strcmp(argv[1], "fork")) {
        loop_fork();
    } else {
        create_process(atoi(argv[1]));
    }
}