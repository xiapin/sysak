//
// Created by 廖肇燕 on 2023/1/12.
//

#include "proto_sender.h"
#define _GNU_SOURCE
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#define PROTO_QUEUE_SIZE  64
#define gettidv1() syscall(__NR_gettid)

extern int lua_reg_errFunc(lua_State *L);
extern int lua_check_ret(int ret);
int lua_load_do_file(lua_State *L, const char* path);

static int call_init(lua_State *L, int err_func, struct beeQ* pushQ) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "init");
    lua_pushlightuserdata(L, pushQ);
    lua_pushinteger(L, (int)gettidv1());
    ret = lua_pcall(L, 2, 1, err_func);
    if (ret) {
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

    ret = lua_load_do_file(L, "../beeQ/proto_send.lua");
    if (ret) {
        goto endLoad;
    }

    lua_register(L, "collector_qout", collector_qout);
    ret = call_init(L, err_func, pushQ);
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
    int err_func = lua_gettop(L);
    struct beeQ* que;

    lua_getglobal(L, "que");
    ret = lua_pcall(L, 0, 1, err_func);
    if (ret) {
        lua_check_ret(ret);
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
int proto_send_proc(void* msg, struct beeQ* q) {
    int ret = 0;
    int err_func;
    struct unity_lines *lines = (struct unity_lines *)msg;
    int num = lines->num;
    struct unity_line * pLine = lines->line;
    static int counter = 0;

    int lret;
    lua_State *L = (lua_State *)(q->qarg);

    if (counter != sighup_counter) {    // check counter for signal.
        struct beeQ* proto_q = proto_que(L);
        lua_close(L);

        L = proto_sender_lua(proto_q);
        if (L == NULL) {
            exit(1);
        }
        q->qarg = L;
        counter = sighup_counter;
    }
    err_func = lua_gettop(L);

    lua_getglobal(L, "send");
    lua_pushnumber(L, num);
    lua_pushlightuserdata(L, pLine);
    ret = lua_pcall(L, 2, 1, err_func);
    if (ret) {
        lua_check_ret(ret);
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
    free(msg);
    return ret;
    endMem:
    endReturn:
    endCall:
    free(msg);
    return ret;
}

// 这是接收外部非lua 推送队列的操作
static int proto_recv_setup(struct beeQ* q) {
    lua_State *L;
    struct beeQ* pushQ = (struct beeQ*)(q->qarg);
    prctl(PR_SET_NAME, (unsigned long)"proto_recv");

    L = proto_sender_lua(pushQ);
    if (L == NULL) {
        return -1;
    }

    q->qarg = (void *)L;
    return 0;
}

struct beeQ* proto_sender_init(struct beeQ* pushQ) {
    struct beeQ* recvQ;

    recvQ = beeQ_init(PROTO_QUEUE_SIZE,
                      proto_recv_setup,
                      proto_send_proc,
                      pushQ);
    if (recvQ == NULL) {
        perror("setup proto queue failed.");
        exit(1);
    }
    return recvQ;
}
