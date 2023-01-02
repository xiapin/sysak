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

extern volatile int sighup_counter;
int app_recv_proc(void* msg, void * arg) {
    int ret = 0;
    struct beeMsg *pMsg = (struct beeMsg *)msg;
    int len = pMsg->size;
    static int counter = 0;

    if (len > 0) {
        int lret;
        lua_State **pL = (lua_State **)arg;
        lua_State *L = *pL;
        char *body;

        if (counter != sighup_counter) {    // check counter for signal.
            lua_close(L);

            L = app_recv_init();
            if (L == NULL) {
                exit(1);
            }
            *pL = L;
            counter = sighup_counter;
        }

        body = malloc(len);   //  http://www.lua.org/manual/5.1/manual.html#lua_pushlstring
        //Pushes the string pointed to by s with size len onto the stack.
        // Lua makes (or reuses) an internal copy of the given string,
        // so the memory at s can be freed or reused immediately after the function returns.
        // The string can contain embedded zeros.
        if (body == NULL) {
            ret = -ENOMEM;
            goto endMem;
        }
        memcpy(body, &pMsg->body[0], len);
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
    endMem:
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

static lua_State * app_collector_init(void* q, int delta) {
    int ret;
    lua_Number lret;

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

    lua_register(L, "collector_qout", collector_qout);

    // call init.
    lua_getglobal(L, "init");
    lua_pushlightuserdata(L, q);
    lua_pushinteger(L, delta);
    ret = lua_pcall(L, 2, 1, 0);
    if (ret) {
        perror("luaL_call init func error");
        report_lua_failed(L);
        goto endCall;
    }

    if (!lua_isnumber(L, -1)) {   // check
        errno = -EINVAL;
        perror("function collectors.lua init must return a number.");
        goto endReturn;
    }
    lret = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if (lret < 0) {
        errno = -EINVAL;
        ret = -1;
        perror("collectors.lua init failed.");
        goto endReturn;
    }
    return L;

    endReturn:
    endCall:
    endLoad:
    lua_close(L);
    endNew:
    return NULL;
}

static int app_collector_work(lua_State **pL, void* q, int delta) {
    int ret;
    lua_Number lret;
    static int counter = 0;

    lua_State *L = *pL;

    if (counter != sighup_counter) {    // check counter for signal.
        lua_close(L);

        L = app_collector_init(q, delta);
        if (L == NULL) {
            exit(1);
        }
        *pL = L;
        counter = sighup_counter;
    }

    lua_getglobal(L, "work");
    lua_pushinteger(L, 15);
    ret = lua_pcall(L, 1, 1, 0);
    if (ret) {
        perror("luaL_call init func error");
        report_lua_failed(L);
        goto endCall;
    }

    if (!lua_isnumber(L, -1)) {   // check
        errno = -EINVAL;
        perror("function collectors.lua work must return a number.");
        goto endReturn;
    }
    lret = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if (lret < 0) {
        errno = -EINVAL;
        ret = -1;
        perror("collectors.lua work failed.");
        goto endReturn;
    }

    return ret;
    endReturn:
    endCall:
    return ret;
}

#include <unistd.h>
#include <time.h>
typedef long bee_time_t;
#define APP_LOOP_PERIOD 15
static bee_time_t local_time(void) {
    int ret;
    struct timespec tp;

    ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (ret == 0) {
        return tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
    } else {
        perror("get clock failed.");
        exit(1);
        return 0;
    }
}

int app_collector_run(struct beeQ* q, void* arg) {
    int ret = 0;
    lua_State *L;
    lua_State **pL;

    L = app_collector_init(q, APP_LOOP_PERIOD);
    if (L == NULL) {
        ret = -1;
        goto endInit;
    }
    pL = &L;

    while (1) {
        bee_time_t t1, t2, delta;
        t1 = local_time();
        ret = app_collector_work(pL, q, APP_LOOP_PERIOD);
        if (ret < 0) {
            goto endLoop;
        }
        t2 = local_time();

        delta = t1 + APP_LOOP_PERIOD * 1000000 - t2;

        if (delta > 0) {
            usleep(delta);
        }
    }

    lua_close(L);
    return 0;
    endLoop:
    endInit:
    return ret;
}
