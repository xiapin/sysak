//
// Created by 廖肇燕 on 2023/3/5.
//

#ifndef UNITY_FFI_UNITY_API_H
#define UNITY_FFI_UNITY_API_H

void ffi_set_unity_proc(const char *path);
void ffi_set_unity_sys(const char *path);
void ffi_plugin_init(void);
void ffi_plugin_stop(void);
void ffi_plugin_deinit(void);

#endif //UNITY_FFI_UNITY_API_H
