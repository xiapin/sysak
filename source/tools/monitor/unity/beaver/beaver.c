//
// Created by 廖肇燕 on 2022/12/20.
//

#include "beaver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <lauxlib.h>
#include <lualib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>

extern int lua_reg_errFunc(lua_State *L);
extern int lua_check_ret(int ret);
int lua_load_do_file(lua_State *L, const char* path);

static int call_init(lua_State *L, int err_func, char *fYaml) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "init");
    lua_pushstring(L, fYaml);
    ret = lua_pcall(L, 1, 1, err_func);
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
        perror("beaver.lua init failed.");
        goto endReturn;
    }

    return ret;
    endReturn:
    endCall:
    return ret;
}

void LuaAddPath(lua_State *L, char *name, char *value) {
    char s[256];

    lua_getglobal(L, "package");
    lua_getfield(L, -1, name);
    strcpy(s, lua_tostring(L, -1));
    strcat(s, ";");
    strcat(s, value);
    strcat(s, ";");
    lua_pushstring(L, s);
    lua_setfield(L, -3, name);
    lua_pop(L, 2);
}

static lua_State * echos_init(char *fYaml) {
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
    LuaAddPath(L, "path", "../beaver/?.lua");
    err_func = lua_reg_errFunc(L);

    ret = lua_load_do_file(L, "../beaver/beaver.lua");
    if (ret) {
        goto endLoad;
    }

    ret = call_init(L, err_func, fYaml);
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

static int echos(lua_State *L) {
    int ret;
    int err_func;
    lua_Number lret;

    err_func = lua_gettop(L);
    lua_getglobal(L, "echo");
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

    return ret;
    endReturn:
    endCall:
    return ret;
}

int beaver_init(char *fYaml) {
    int ret = 0;

    while (ret == 0) {
        lua_State *L = echos_init(fYaml);
        if (L == NULL) {
            break;
        }
        ret = echos(L);
        lua_close(L);
    }
    exit(1);
}
