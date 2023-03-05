//
// Created by 廖肇燕 on 2023/2/16.
//

#include "outline.h"
#include <errno.h>

extern int lua_reg_errFunc(lua_State *L);
extern int lua_check_ret(int ret);
int lua_load_do_file(lua_State *L, const char* path);

static int call_init(lua_State *L, int err_func, void* q, char *fYaml) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "init");
    lua_pushlightuserdata(L, q);
    lua_pushstring(L, fYaml);
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

extern int collector_qout(lua_State *L);
static lua_State * pipe_init(void* q, char *fYaml) {
    int ret;
    int err_func;
    lua_Number lret;

    /* create a state and load standard library. */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        perror("new lua for out line failed.");
        goto endNew;
    }
    luaL_openlibs(L);
    err_func = lua_reg_errFunc(L);

    ret = lua_load_do_file(L, "../beeQ/outline.lua");
    if (ret) {
        goto endLoad;
    }

    lua_register(L, "collector_qout", collector_qout);
    ret = call_init(L, err_func, q, fYaml);
    if (ret) {
        lua_check_ret(ret);
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
    int err_func;
    lua_Number lret;

    err_func = lua_gettop(L);
    lua_getglobal(L, "work");
    ret = lua_pcall(L, 0, 1, err_func);
    if (ret) {
        lua_check_ret(ret);
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
    lua_close(L);

    return ret;
    endReturn:
    endCall:
    lua_close(L);
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

pthread_t outline_init(struct beeQ* pushQ, char *fYaml) {
    pthread_t tid = 0;

    tid = beeQ_send_thread(pushQ, fYaml, outline_run);
    return tid;
}
