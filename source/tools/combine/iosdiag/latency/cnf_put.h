#ifndef _CNF_PUT_H
#define _CNF_PUT_H

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "format_json.h"

#define PIPE_PATH "/var/sysom/outline"
#define MAX_BUFF 128*1024
#define UPLOAD_INTERVAL 3

extern int upload_num;
extern int upload_capacity; 
extern char** upload_array;
pthread_mutex_t upload_mutex;

struct cnfPut {
    int _sock;
    struct sockaddr_un _server_addr;
};

int cnfPut_init(struct cnfPut* self, const char* path);
int cnfPut_puts(struct cnfPut* self, const char* s);
void cnfPut_destroy(struct cnfPut *cnfput);
void reset_upload_statistics();
void expand_upload_array();

#endif

