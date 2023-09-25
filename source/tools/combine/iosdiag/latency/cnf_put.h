#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "format_json.h"

#define PIPE_PATH "/var/sysom/outline"
#define MAX_BUFF 128*1024

#define PIPE_PATH "/var/sysom/outline"
#define MAX_BUFF 128*1024

struct cnfPut {
    int _sock;
    struct sockaddr_un _server_addr;
};

int cnfPut_init(struct cnfPut* self, const char* path);
int cnfPut_puts(struct cnfPut* self, const char* s);
void cnfPut_destroy(struct cnfPut *cnfput);