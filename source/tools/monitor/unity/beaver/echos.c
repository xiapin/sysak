//
// Created by 廖肇燕 on 2022/12/20.
//

#include "echos.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <lauxlib.h>
#include <lualib.h>
#include <errno.h>

LUALIB_API void luaL_traceback (lua_State *L, lua_State *L1, const char *msg, int level);

static void report_lua_failed(lua_State *L) {
    fprintf(stderr, "\nFATAL ERROR:%s\n\n", lua_tostring(L, -1));
}

static int call_init(lua_State *L, int tid) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "init");
    lua_pushinteger(L, tid);
    ret = lua_pcall(L, 1, 1, 0);
    if (ret) {
        perror("luaL_call init func error");
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

lua_State * echos_init(int tid) {
    int ret;

    /* create a state and load standard library. */
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        perror("new lua failed.");
        goto endNew;
    }

    /* opens all standard Lua libraries into the given state. */
    luaL_openlibs(L);

    LuaAddPath(L, "path", "../beaver/?.lua");

    ret = luaL_loadfile(L, "../beaver/beaver.lua");
    printf("ret: %d\n", ret);
    ret = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (ret) {
        const char *msg = lua_tostring(L, -1);
        perror("luaL_dofile error");
        if (msg) {
            luaL_traceback(L, L, msg, 0);
            fprintf(stderr, "FATAL ERROR:%s\n\n", msg);
        }
        goto endLoad;
    }

    ret = call_init(L, tid);
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

int echos(lua_State *L, int fd) {
    int ret;
    lua_Number lret;

    lua_getglobal(L, "echo");
    lua_pushinteger(L, fd);
    ret = lua_pcall(L, 1, 1, 0);
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
        perror("beaver.lua init failed.");
        goto endReturn;
    }

    return ret;
    endReturn:
    endCall:
    return ret;
}