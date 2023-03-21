//
// Created by 廖肇燕 on 2023/3/5.
//

#include "ffi_unity_api.h"
#include "../interface/unity_interface.h"
#include "../interface/sig_stop.h"

void ffi_set_unity_proc(const char *path) {
    set_unity_proc(path);
}

void ffi_set_unity_sys(const char *path) {
    set_unity_sys(path);
}

void ffi_plugin_init(void) {
    plugin_init();
}

void ffi_plugin_stop(void) {
    plugin_stop();
}

void ffi_plugin_deinit(void) {
    plugin_deinit();
}
