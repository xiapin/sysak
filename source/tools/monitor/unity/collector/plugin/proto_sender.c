//
// Created by 廖肇燕 on 2023/1/12.
//

#include "proto_sender.h"
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#define PROTO_QUEUE_SIZE  64
#define gettidv1() syscall(__NR_gettid)

LUALIB_API void luaL_traceback(lua_State *L, lua_State *L1, const char *msg, int level);

static void report_lua_failed(lua_State *L) {
    fprintf(stderr, "\nFATAL ERROR:%s\n\n", lua_tostring(L, -1));
}

static int call_init(lua_State *L, struct beeQ* pushQ) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "init");
    lua_pushlightuserdata(L, pushQ);
    lua_pushinteger(L, (int)gettidv1());
    ret = lua_pcall(L, 2, 1, 0);
    if (ret) {
        perror("proto_sender lua init func error");
        report_lua_failed(L);
        goto endCall;
    }

    if (!lua_isnumber(L, -1)) {   // check
        errno = -EINVAL;
        perror("function proto_send.lua init must return a number.");
        goto endReturn;
    }
    lret = lua_tonumber(L, -1);
    lua_pop(L, 1);
    if (lret < 0) {
        errno = -EINVAL;
        ret = -1;
        perror("proto_send.lua init failed.");
        goto endReturn;
    }

    return ret;
    endReturn:
    endCall:
    return ret;
}

extern int collector_qout(lua_State *L);
lua_State * proto_sender_lua(struct beeQ* pushQ)  {
    int ret;

    /* create a state and load standard library. */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        perror("new lua failed.");
        goto endNew;
    }
    /* opens all standard Lua libraries into the given state. */
    luaL_openlibs(L);

    ret = luaL_dofile(L, "proto_send.lua");
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
    ret = call_init(L, pushQ);
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

struct beeQ* proto_que(lua_State *L) {
    int ret;
    struct beeQ* que;

    lua_getglobal(L, "que");
    ret = lua_pcall(L, 0, 1, 0);
    if (ret) {
        perror("proto_que lua que func error");
        report_lua_failed(L);
        goto endCall;
    }
    if (!lua_isuserdata(L, -1)) {   // check
        errno = -EINVAL;
        perror("function proto_send.lua init must return a number.");
        goto endReturn;
    }
    que = (struct beeQ*)lua_topointer(L, -1);
    lua_pop(L, 1);
    if (que == NULL) {
        errno = -EINVAL;
        ret = -1;
        perror("proto_send.lua init failed.");
        goto endReturn;
    }

    return que;
    endReturn:
    endCall:
    return NULL;

}

extern volatile int sighup_counter;
int proto_send_proc(void* msg, void * arg) {
    int ret = 0;
    struct unity_lines *lines = (struct unity_lines *)msg;
    int num = lines->num;
    struct unity_line * pLine = lines->line;
    static int counter = 0;

    int lret;
    lua_State **pL = (lua_State **)arg;
    lua_State *L = *pL;

    free(lines);  // free message head at first

    if (counter != sighup_counter) {    // check counter for signal.
        struct beeQ* que = proto_que(L);
        lua_close(L);

        L = proto_sender_lua(que);
        if (L == NULL) {
            exit(1);
        }
        *pL = L;
        counter = sighup_counter;
    }

    lua_getglobal(L, "send");
    lua_pushnumber(L, num);
    lua_pushlightuserdata(L, pLine);
    ret = lua_pcall(L, 2, 1, 0);
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
    return ret;
    endMem:
    endReturn:
    endCall:
    return ret;
}

struct beeQ* proto_sender_init(struct beeQ* pushQ) {
    struct beeQ* recvQ;
    lua_State *L;
    lua_State **pL;

    L = proto_sender_lua(pushQ);
    if (L == NULL) {
        exit(1);
    }

    pL = &L;
    recvQ = beeQ_init(PROTO_QUEUE_SIZE, proto_send_proc, (void *)pL);
    if (recvQ == NULL) {
        perror("setup proto queue failed.");
        exit(1);
    }
    return recvQ;
}
