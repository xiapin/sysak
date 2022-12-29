//
// Created by 廖肇燕 on 2022/12/26.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "beeQ.h"
#include "apps.h"
#include <sys/syscall.h>

#define gettidv1() syscall(__NR_gettid)

LUALIB_API void luaL_traceback (lua_State *L, lua_State *L1, const char *msg, int level);

static void report_lua_failed(lua_State *L) {
    fprintf(stderr, "\nFATAL ERROR:%s\n\n", lua_tostring(L, -1));
}

static int call_init(lua_State *L) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "init");
    lua_pushinteger(L, (int)gettidv1());
    ret = lua_pcall(L, 1, 1, 0);
    if (ret) {
        perror("luaL_call init func error");
        report_lua_failed(L);
        goto endCall;
    }

    if (!lua_isnumber(L, -1)) {   // check
        errno = -EINVAL;
        perror("function bees.lua init must return a number.");
        goto endReturn;
    }
    lret = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if (lret < 0) {
        errno = -EINVAL;
        ret = -1;
        perror("bees.lua init failed.");
        goto endReturn;
    }

    return ret;
    endReturn:
    endCall:
    return ret;
}

lua_State * app_recv_init(void)  {
    int ret;

    /* create a state and load standard library. */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        perror("new lua failed.");
        goto endNew;
    }
    /* opens all standard Lua libraries into the given state. */
    luaL_openlibs(L);

    ret = luaL_dofile(L, "bees.lua");
    if (ret) {
        const char *msg = lua_tostring(L, -1);
        perror("luaL_dofile error");
        if (msg) {
            luaL_traceback(L, L, msg, 0);
            fprintf(stderr, "FATAL ERROR:%s\n\n", msg);
        }
        goto endLoad;
    }

    ret = call_init(L);
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

int app_recv_proc(void* msg, void * arg) {
    int ret = 0;
    struct beeMsg *pMsg = (struct beeMsg *)msg;
    int len = pMsg->size;
    char * body = &pMsg->body[0];
    if (len > 0) {
        int lret;
        lua_State *L = (lua_State *)arg;
        lua_getglobal(L, "proc");
        lua_pushlstring(L, body, len);
        ret = lua_pcall(L, 1, 1, 0);
        if (ret) {
            perror("lua call error");
            report_lua_failed(L);
            goto endCall;
        }

        if (!lua_isnumber(L, -1)) {   // check
            errno = -EINVAL;
            perror("function bees.lua proc must return a number.");
            goto endReturn;
        }
        lret = lua_tonumber(L, -1);
        lua_pop(L, 1);
        if (lret < 0) {
            errno = -EINVAL;
            ret = -1;
            perror("bees.lua proc failed.");
            goto endReturn;
        }
    }
    return ret;
    endReturn:
    endCall:
    return ret;
}

static int collector_qout(lua_State *L) {
    int ret;
    struct beeQ* q = (struct beeQ*) lua_topointer(L, 1);
    const char *body = lua_tostring(L, 2);
    int len = lua_tonumber(L, 3);

    struct beeMsg* pMsg = malloc(sizeof (int) + len);
    if (pMsg == NULL) {
        lua_pushnumber(L, -ENOMEM);
        return 1;
    }

    pMsg->size = len;
    memcpy(&pMsg->body[0], body, len);

    ret = beeQ_send(q, pMsg);
    lua_pushnumber(L, ret);
    return 1;
}

static int app_collector_loop(lua_State *L, void* q) {
    int ret;
    lua_Number lret;

    lua_register(L, "collector_qout", collector_qout);

    lua_getglobal(L, "run");
    lua_pushlightuserdata(L, q);
    lua_pushinteger(L, 15);
    ret = lua_pcall(L, 2, 1, 0);
    if (ret) {
        perror("luaL_call init func error");
        report_lua_failed(L);
        goto endCall;
    }

    if (!lua_isnumber(L, -1)) {   // check
        errno = -EINVAL;
        perror("function bees.lua init must return a number.");
        goto endReturn;
    }
    lret = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if (lret < 0) {
        errno = -EINVAL;
        ret = -1;
        perror("bees.lua init failed.");
        goto endReturn;
    }

    return ret;
    endReturn:
    endCall:
    return ret;
}

int app_collector_run(struct beeQ* q, void* arg) {
    int ret;

    /* create a state and load standard library. */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        perror("new lua failed.");
        goto endNew;
    }
    luaL_openlibs(L);

    ret = luaL_dofile(L, "collectors.lua");
    if (ret) {
        const char *msg = lua_tostring(L, -1);
        perror("luaL_dofile error");
        if (msg) {
            luaL_traceback(L, L, msg, 0);
            fprintf(stderr, "FATAL ERROR:%s\n\n", msg);
        }
        goto endLoad;
    }

    ret = app_collector_loop(L, q);
    if (ret < 0) {
        goto endCall;
    }

    lua_close(L);
    return 0;
    endCall:
    endLoad:
    lua_close(L);
    endNew:
    return ret;
}
