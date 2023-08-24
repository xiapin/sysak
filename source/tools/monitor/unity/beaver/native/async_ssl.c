//
// Created by 廖肇燕 on 2023/8/21.
//

#include "async_ssl.h"
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define BUFF_MAX 16384
static SSL_CTX *sslContext = NULL;

int ssl_read(void *handle, char **pp, int len)
{
    int ret = 0;
    int size = BUFF_MAX < len ? BUFF_MAX : len;
    char *p = malloc(size);

    if (p == NULL) {
        ret = -ENOMEM;
        fprintf(stderr, "ssl read malloc failed. %d, %s\n", errno, strerror(errno));
        goto nomem;
    }

    ret = SSL_read((SSL *)handle, p, size);
    if (ret < 0) {
        int err = SSL_get_error((SSL *)handle, ret);
        if (err == SSL_ERROR_WANT_READ) {
            free(p);
            ret = 0;
            goto needContinue;
        }
        goto readFailed;
    }

    *pp = p;
    return ret;
    needContinue:   // for to continue read.
    return ret;
    readFailed:
    free(p);
    nomem:
    return ret;
}

void ssl_free_buff(char ** pp) {
    char *p = *pp;
    free((void *)p);
}

int ssl_write(void *handle, const char *buff, int len) {
    int ret = 0;
    ret = SSL_write((SSL *)handle, buff, len);

    if (ret < 0) {
        int err = SSL_get_error((SSL *)handle, ret);
        if (err == SSL_ERROR_WANT_WRITE) {  //just need to write.
            ret = 0;
            goto needContinue;
        }
    }
    return ret;
    needContinue:
    return ret;
}

void *ssl_connect_pre(int fd) {
    int ret;
    SSL *handle = SSL_new(sslContext);
    if (handle == NULL) {
        fprintf(stderr, "ssl_connect_pre new ssl failed. %d, %s\n", errno, strerror(errno));
        return NULL;
    }

    ret = SSL_set_fd(handle, fd);
    if (ret < 0) {
        fprintf(stderr, "ssl_connect_pre bind fd failed. %d, %s\n", errno, strerror(errno));
        SSL_shutdown(handle);
        SSL_free(handle);
        return NULL;
    }
    SSL_set_connect_state(handle);
    return handle;
}

int ssl_connect(void * handle) {
    int ret = 0, err = 0;
    SSL *h = (SSL *)handle;

    ret = SSL_do_handshake(h);
    if (ret == 1) {
        return 0;  // means handshake  success.
    }

    err = SSL_get_error(h, ret);
    switch (err) {
        case SSL_ERROR_WANT_WRITE:  //waite write.
            return 1;
        case SSL_ERROR_WANT_READ:
            return 2;
        default:
            fprintf(stderr, "ssl_connect handshake failed. %d, %s\n", errno, strerror(errno));
            return -1;
    }
}

void ssl_del(void *handle) {
    SSL *h = (SSL *)handle;
    SSL_shutdown(handle);
    SSL_free(handle);
}

int async_ssl_init(void) {
    int ret = 0;

    SSL_load_error_strings();
    SSL_library_init();
    sslContext = SSL_CTX_new(SSLv23_client_method());
    if (sslContext == NULL) {
        fprintf(stderr, "set up sslContext failed. %d, %s\n", errno, strerror(errno));
        ret = -errno;
        goto sslFailed;
    }
    return ret;

    sslFailed:
    return ret;
}

void async_ssl_deinit(void) {
    SSL_CTX_free(sslContext);
    sslContext = NULL;
}
