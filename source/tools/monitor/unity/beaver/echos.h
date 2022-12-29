//
// Created by 廖肇燕 on 2022/12/20.
//

#ifndef UNITY_ECHOS_H
#define UNITY_ECHOS_H
#include <lua.h>

#define BEAVER_DEBUG
lua_State * echos_init(int tid);
int echos(lua_State *L, int fd);

#endif //UNITY_ECHOS_H
