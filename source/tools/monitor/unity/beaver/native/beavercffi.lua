---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/2/14 2:59 PM
---

local ffi = require("ffi")
local cffi = ffi.load('lbeaver')
ffi.cdef [[
typedef struct native_event {
    int fd;
    short int ev_in;
    short int ev_out;
    short int ev_close;
}native_event_t;

typedef struct native_events {
    int num;
    native_event_t evs[64];
}native_events_t;

int init(int listen_fd);
int add_fd(int efd, int fd);
int mod_fd(int efd, int fd, int wr);
int del_fd(int efd, int fd);
int poll_fds(int efd, int tmo, native_events_t* nes);
int setsockopt_AP(int fd);
void deinit(int efd);
]]

return {ffi = ffi, cffi=cffi}
