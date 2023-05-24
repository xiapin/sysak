//
// Created by 廖肇燕 on 2022/12/26.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "beeQ.h"
#include "apps.h"
#include "pushTo.h"
#include "clock/ee_clock.h"
#include "daemon.h"
#include <sys/syscall.h>
#include <sys/prctl.h>

#define gettidv1() syscall(__NR_gettid)
extern char *g_yaml_file;

static int lua_traceback(lua_State *L)
{
    const char *errmsg = lua_tostring(L, -1);
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_call(L, 0, 1);
    printf("%s \n%s\n", errmsg, lua_tostring(L, -1));
    return 1;
}

int lua_reg_errFunc(lua_State *L) {
    lua_pushcfunction(L, lua_traceback);
    return lua_gettop(L);
}

int lua_check_ret(int ret) {
    switch (ret) {
        case 0:
            break;
        case LUA_ERRRUN:
            printf("lua runtime error.\n");
            break;
        case LUA_ERRMEM:
            printf("lua memory error.\n");
        case LUA_ERRERR:
            printf("lua exec error.\n");
        case LUA_ERRSYNTAX:
            printf("file syntax error.\n");
        case LUA_ERRFILE:
            printf("load lua file error.\n");
        default:
            printf("bad res for %d\n", ret);
            exit(1);
    }
    return ret;
}

int lua_load_do_file(lua_State *L, const char* path) {
    int err_func = lua_gettop(L);
    int ret;

    ret = luaL_loadfile(L, path);
    if (ret) {
        return lua_check_ret(ret);
    }
    ret = lua_pcall(L, 0, LUA_MULTRET, err_func);
    return lua_check_ret(ret);
}

static int lua_push_start(lua_State *L) {
    int ret;
    int fd = lua_tonumber(L, 1);
    ret = pushTo_start(fd);
    lua_pushnumber(L, ret);
    return 1;
}

static int lua_push_stop(lua_State *L) {
    pushTo_stop();
    return 0;
}

static int call_init(lua_State *L, int err_func) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "init");
    lua_pushinteger(L, (int)gettidv1());
    lua_pushstring(L, g_yaml_file);
    ret = lua_pcall(L, 2, 1, err_func);
    if (ret) {
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

static lua_State * app_recv_init(void)  {
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

    lua_register(L, "lua_push_start", lua_push_start);
    lua_register(L, "lua_push_stop", lua_push_stop);

    ret = lua_load_do_file(L, "../beeQ/bees.lua");
    if (ret) {
        goto endLoad;
    }

    ret = call_init(L, err_func);
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

// 这是接收队列初始化动作
int app_recv_setup(struct beeQ* q) {
    lua_State *L;

    prctl(PR_SET_NAME, (unsigned long)"app_recv");
    L = app_recv_init();
    if (L == NULL) {
        return -1;
    }
    q->qarg = L;
    return 0;
}

extern volatile int sighup_counter;
int app_recv_proc(void* msg, struct beeQ* q) {
    int ret = 0;
    struct beeMsg *pMsg = (struct beeMsg *)msg;
    int len = pMsg->size;
    static int counter = 0;

    if (len > 0) {
        int lret;
        lua_State *L = (lua_State *)(q->qarg);
        char *body;
        int err_func;

        if (counter != sighup_counter) {    // check counter for signal.
            lua_close(L);

            L = app_recv_init();
            if (L == NULL) {
                exit(1);
            }
            q->qarg = L;
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
        err_func = lua_gettop(L);
        lua_getglobal(L, "proc");
        lua_pushlstring(L, body, len);
        ret = lua_pcall(L, 1, 1, err_func);
        free(body);
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
    }
    endTest:
    free(msg);
    return ret;
    endMem:
    endReturn:
    endCall:
    free(msg);
    exit(1);
    return ret;
}

static int lua_local_clock(lua_State *L) {
    clock_t t = get_local_clock();
    lua_pushnumber(L, t);
    return 1;
}

static int lua_setup_daemon(lua_State *L) {
    int fd;
    int period = lua_tonumber(L, 1);
    fd = setup_daemon(period);
    lua_pushnumber(L, fd);
    return 1;
}

static int prctl_death_kill(lua_State *L) {
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    return 0;
}

int collector_qout(lua_State *L) {
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
    return 1;   // return a value.
}

// 这是 collector 初始化操作
static int app_collector_work(void* q, void* proto_q) {
    int ret;
    int err_func;
    lua_Number lret;

    prctl(PR_SET_NAME, (unsigned long)"app_collector");

    /* create a state and load standard library. */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        perror("new lua for collector failed.");
        goto endNew;
    }
    luaL_openlibs(L);
    err_func = lua_reg_errFunc(L);

    lua_register(L, "collector_qout", collector_qout);
    lua_register(L, "lua_local_clock", lua_local_clock);
    lua_register(L, "lua_setup_daemon", lua_setup_daemon);
    lua_register(L, "prctl_death_kill", prctl_death_kill);

    ret = lua_load_do_file(L, "../beeQ/collectors.lua");
    if (ret) {
        goto endLoad;
    }

    // call init.
    lua_getglobal(L, "work");
    lua_pushlightuserdata(L, q);
    lua_pushlightuserdata(L, proto_q);
    lua_pushstring(L, g_yaml_file);
    lua_pushinteger(L, (int)gettidv1());
    ret = lua_pcall(L, 4, 1, err_func);
    if (ret < 0) {
        lua_check_ret(ret);
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
        perror("collectors.lua work failed.");
        goto endReturn;
    }
    lua_close(L);
    return lret;

    endReturn:
    endCall:
    endLoad:
    lua_close(L);
    endNew:
    return -1;
}


int app_collector_run(struct beeQ* q, void* arg) {
    int ret = 0;
    struct beeQ* proto_que = (struct beeQ* )arg;

    while (1) {
        ret = app_collector_work(q, proto_que);
        if (ret < 0) {
            perror("collect work run failed.");
            exit(1);
            break;
        }
    }
    return ret;
}
