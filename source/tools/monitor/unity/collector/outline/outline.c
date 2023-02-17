//
// Created by 廖肇燕 on 2023/2/16.
//

#include "outline.h"
#include <errno.h>

LUALIB_API void luaL_traceback (lua_State *L, lua_State *L1, const char *msg, int level);

static void report_lua_failed(lua_State *L) {
    fprintf(stderr, "\nFATAL ERROR:%s\n\n", lua_tostring(L, -1));
}

static int call_init(lua_State *L, void* q, char *fYaml) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "init");
    lua_pushlightuserdata(L, q);
    lua_pushstring(L, fYaml);
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

extern int collector_qout(lua_State *L);
static lua_State * pipe_init(void* q, char *fYaml) {
    int ret;
    lua_Number lret;

    /* create a state and load standard library. */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        perror("new lua for out line failed.");
        goto endNew;
    }
    luaL_openlibs(L);

    ret = luaL_dofile(L, "outline.lua");
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
    ret = call_init(L, q, fYaml);
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

static int work(lua_State *L) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "work");
    ret = lua_pcall(L, 0, 1, 0);
    if (ret) {
        perror("lua call error");
        report_lua_failed(L);
        goto endCall;
    }

    if (!lua_isnumber(L, -1)) {   // check
        errno = -EINVAL;
        perror("function beaver.lua init must return a number.");
        goto endReturn;
    }
    lret = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if (lret < 0) {
        errno = -EINVAL;
        ret = -1;
        perror("beaver.lua echo failed.");
        goto endReturn;
    }

    return ret;
    endReturn:
    endCall:
    return ret;
}

static int outline_work(struct beeQ* q, char *fYaml) {
    lua_State *L;

    L = pipe_init(q, fYaml);
    if (L == NULL) {
        return -1;
    }

    return work(L);
}

static int outline_run(struct beeQ* q, void* arg) {
    int ret;
    char *fYaml = (char *)arg;

    while (1) {
        ret = outline_work(q, fYaml);
        if (ret < 0) {
            break;
        }
    }
    return ret;
}

int outline_init(struct beeQ* pushQ, char *fYaml) {
    pthread_t tid;

    tid = beeQ_send_thread(pushQ, fYaml, outline_run);
    return tid;
}
