//
// Created by 廖肇燕 on 2022/12/31.
//

#include <lauxlib.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    luaL_dofile(L, "foxTest.lua");
    lua_getglobal(L, "test");
    lua_pcall(L, 0, 0, 0);
    lua_close(L);
    printf("OK.\n");
    return 0;
}
