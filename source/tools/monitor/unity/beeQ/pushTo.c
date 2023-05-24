//
// Created by 廖肇燕 on 2023/4/7.
//

#include "pushTo.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <lauxlib.h>
#include <lualib.h>

extern char *g_yaml_file;
static int fdIn;
static pthread_t pid_push = 0;

extern int lua_reg_errFunc(lua_State *L);
extern int lua_check_ret(int ret);
int lua_load_do_file(lua_State *L, const char* path);

static int call_work(lua_State *L, int err_func, int fd, char *fYaml) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "work");
    lua_pushinteger(L, fd);
    lua_pushstring(L, fYaml);
    ret = lua_pcall(L, 2, 1, err_func);
    if (ret) {
        goto endCall;
    }
    if (!lua_isnumber(L, -1)) {   // check
        errno = -EINVAL;
        perror("function pushTo.lua init must return a number.");
        goto endReturn;
    }
    lret = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if (lret < 0) {
        errno = -EINVAL;
        ret = -1;
        perror("pushTo.lua init failed.");
        goto endReturn;
    }

    return ret;
    endReturn:
    endCall:
    return ret;
}

static lua_State * push_init(int fd, char* fYaml)  {
    int ret;
    int err_func;
    /* create a state and load standard library. */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        perror("new lua failed.");
        goto endNew;
    }
    /* opens all standard Lua libraries into the given state. */
    luaL_openlibs(L);
    err_func = lua_reg_errFunc(L);

    ret = lua_load_do_file(L, "../beeQ/pushTo.lua");
    if (ret) {
        goto endLoad;
    }

    ret = call_work(L, err_func, fd, fYaml);
    if (ret < 0) {
        goto endCall;
    }

    return L;
    endCall:
    endLoad:
    lua_close(L);
    endNew:
    return NULL;
}

static int push_work(int fd, char *fYaml) {
    push_init(fd, fYaml);
    return 0;
}


static void* pushTo_worker(void *arg) {
    int fd = fdIn;

    push_work(fd, g_yaml_file);
    return NULL;
}

int pushTo_start(int fd) {
    int ret;
    if (fd > 0) {
        fdIn = fd;
        ret = pthread_create(&pid_push, NULL, pushTo_worker, NULL);
        if (ret < 0) {
            perror("create push thread failed");
            exit(1);
        }
        return 0;
    }
    return -EINVAL;
}

void pushTo_stop(void) {
    pthread_kill(pid_push, SIGUSR2);
    pthread_join(pid_push, NULL);
    pid_push = 0;
}
