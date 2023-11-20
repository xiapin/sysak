#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "cnf_put.h"

int upload_num = 0;
int upload_capacity = 10; 
char** upload_array;

int cnfPut_init(struct cnfPut* self, const char* path) {
    self->_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (self->_sock == -1) {
        fprintf(stderr, "pipe path is not exist. please check Netinfo is running.\n");
        return -1;
    }
    memset(&self->_server_addr, 0, sizeof(self->_server_addr));
    self->_server_addr.sun_family = AF_UNIX;
    strncpy(self->_server_addr.sun_path, PIPE_PATH, sizeof(self->_server_addr.sun_path) - 1);
	return 0;
}

int cnfPut_puts(struct cnfPut* self, const char* s) {
    if (strlen(s) > MAX_BUFF) {
        fprintf(stderr, "message len %zu is too long, should be less than %d\n", strlen(s), MAX_BUFF);
        return -1;
    }

    if (connect(self->_sock, (struct sockaddr*)&self->_server_addr, sizeof(self->_server_addr)) == -1) {
        return -1;
    }

    if (send(self->_sock, s, strlen(s), 0) == -1) {
        fprintf(stderr, "send %s fail.\n", s);
        return -1;
    }
	return 0;
}

void cnfPut_destroy(struct cnfPut *cnfput) {
    if (cnfput->_sock != -1) {
        close(cnfput->_sock);
        cnfput->_sock = -1;
    }
}

void reset_upload_statistics() 
{
	upload_num = 0;
	upload_capacity = 10;
	upload_array = malloc(upload_capacity * sizeof(char*));
}

void expand_upload_array() 
{
    upload_capacity *= 2; 
    upload_array = realloc(upload_array, upload_capacity * sizeof(char*));
}
