#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

void loop_fork() {
    while (1) {
        int pid[128];
        for (int i = 0; i < 128; i++) {
            if ((pid[i] = fork()) == 0) {
                exit(0);
            }
            // printf("fork.\n");
        }

        for (int i = 0; i < 128; i++) {
            waitpid(pid[i], 0, 0);
        }
    }
}

void *do_nothing(void *arg) { return 0; }

void loop_clone() {
    while (1) {
        pthread_t pid[128];
        for (int i = 0; i < 128; i++) {
            pthread_create(&pid[i], 0, do_nothing, 0);
            // printf("fork.\n");
        }

        for (int i = 0; i < 128; i++) {
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
    }
}