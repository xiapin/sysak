---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/30 10:20 AM
---

require("common.class")
local Cplugin = class("plugin")
local dockerinfo = require("common.dockerinfo")

function Cplugin:_init_(resYaml, ffi, proto_q, so)
    self._ffi = ffi
    self._cffi = self._ffi.load(so)
    self._cffi.init(proto_q)
    self._so = so
    self.proc_fs = resYaml.config["proc_path"] or "/"
    self.fill_arg = {["podname"]="pid"}
end

function Cplugin:_del_()
    print("uninstall " .. self._so)
    self._cffi.deinit()
end

function Cplugin:load_label(unity_line, line)

    local c = #line.ls
    local table_dict = {}

    for i=0, 4 - 1 do
        local name = self._ffi.string(unity_line.indexs[i].name)
        local index = self._ffi.string(unity_line.indexs[i].index)
        table_dict[name] = index
    end

    for i=0, 4 - 1 do
        local name = self._ffi.string(unity_line.indexs[i].name)
        local index = self._ffi.string(unity_line.indexs[i].index)

        if #name > 0 then
            c = c + 1
            if index == "?" and self.fill_arg[name] and table_dict[self.fill_arg[name]] then
                if name == "podname" then
                    local podname = dockerinfo:get_podname_pid(table_dict[self.fill_arg[name]], self.proc_fs) 
                    line.ls[c] = {name = name, index = podname}
                end
            else
                line.ls[c] = {name = name, index = index}
            end
        else
            return
        end
    end
end

function Cplugin:load_value(unity_line, line)
    local c = #line.vs
    for i=0, 32 - 1 do
        local name = self._ffi.string(unity_line.values[i].name)
        local value = unity_line.values[i].value

        if #name > 0 then
            c = c + 1
            line.vs[c] = {name = name, value = value}
        else
            return
        end
    end
end

function Cplugin:load_log(unity_line, line)
    local name = self._ffi.string(unity_line.logs[0].name)
    if #name > 0 then
        local log = self._ffi.string(unity_line.logs[0].log)
        self._ffi.C.free(unity_line.logs[0].log)   -- should free from strdup
        table.insert(line.log, {name = name, log = log})
    end
end

function Cplugin:_proc(unity_lines, lines)
    local c = #lines["lines"]
    for i=0, unity_lines.num - 1 do
        local unity_line = unity_lines.line[i]
        local line = {line = self._ffi.string(unity_line.table),
                      ls = {},
                      vs = {},
                      log = {}}

        self:load_label(unity_line, line)
        self:load_value(unity_line, line)
        self:load_log(unity_line, line)
        c = c + 1
        lines["lines"][c] = line
    end
end

function Cplugin:proc(t, lines)
    local unity_lines = self._ffi.new("struct unity_lines")
    local res = self._cffi.call(t, unity_lines)
    if res == 0 then
        self:_proc(unity_lines, lines)
    end
    self._ffi.C.free(unity_lines.line)   -- should free memory.
end

return Cplugin
