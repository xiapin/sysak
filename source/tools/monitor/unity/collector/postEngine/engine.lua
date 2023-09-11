---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2023/3/25 4:21 PM
---

require("common.class")
local CprotoData = require("common.protoData")
local postQue = require("beeQ.postQue.postQue")
local CvProto = require("collector.vproto")
local pystring = require("common.pystring")
local system = require("common.system")
local cjson = require("cjson.safe")
local CexecBase = require("collector.postEngine.execBase")
local CexecDiag = require("collector.postEngine.execDiag")
local CexecJobs = require("collector.postEngine.execJobs")
local ChttpReq = require("httplib.httpReq")
local Cengine = class("engine", CvProto)

local diagExec = {
    io_hang = {block = 60, time = 15, cmd = "../../../iosdiag",
               report = {title = "iosdiag",
                         files = {"/var/log/sysak/iosdiag/hangdetect/result.log.stat",
                                  "/var/log/sysak/iosdiag/hangdetect/result.log.seq"}}},
    net_edge = {block = 5 * 60, time = 60, so = {virtiostat = 5 * 3}},
}



function Cengine:_init_(que, proto_q, fYaml, tid)
    CvProto._init_(self, CprotoData.new(que))
    self._que = que
    self._fYaml = fYaml
    self._tid  = tid
    self._task = nil

    local res = system:parseYaml(fYaml)
    self._resDiag = res.diagnose
    self._diags = {}

    if self._resDiag then
        self._fYamlJobs = res.diagnose.jobs
        self._jobs = {}
        self._auth = res.diagnose.token
        self._host = res.diagnose.host
    end

end

function Cengine:setMainloop(main)
    self._main = main
end

function Cengine:setTask(taskMons)
    self._task = taskMons
end

function Cengine:postReq(s, data)
    local req = ChttpReq.new()
    local url = self._host .. "/api/v1/tasks/sbs_task_result/"
    local formData = {
        task_id = data.task_id,
        results = s
    }
    local headers = {
        accept = "application/json",
        ["Content-Type"] = "multipart/form-data",
        authorization = self._auth
    }
    req:postFormData(url,headers,formData)
end

function Cengine:postReqFile(s, data)
    local req = ChttpReq.new()
    local url = self._host .. "/api/v1/tasks/sbs_task_result/"
    local formData = {
        task_id = data.task_id,
        files = s,
        results = ""
    }
    local headers = {
        accept = "application/json",
        ["Content-Type"] = "multipart/form-data",
        authorization = self._auth
    }
    req:postFormData(url,headers,formData)
end

function Cengine:run(e, res, diag)
    local args = res.args
    local second = res.second or diag.time
    if diag.cmd then
        local exec = CexecDiag.new(diag.cmd, args, second, self._que, diag.report, res.uid)
        exec:addEvents(e)
    end
    local so = diag.so
    if so then
        for plugin, loop in pairs(so) do
            print(plugin, loop)
            self._main.soPlugins:add(plugin, loop)
        end
    end
    self._diags[res.exec] = diag.block
end

function Cengine:runJobs(e, res, diag)
    local cmd = res.jobs[1].cmd
    local isFile = false
    local filename
    local filepath
    if #res.jobs[1].fetch_file_list~=0  then
        isFile = true
        filename = res.jobs[1].fetch_file_list[1].name
        filepath = res.jobs[1].fetch_file_list[1].remote_path
    end

    local time
    if diag and diag.time then
        time = diag.time
    else
        time = 30
    end

    if cmd then
        local exec = CexecJobs.new("/bin/bash", {"-c",cmd}, time, res.service_name)
        --local exec = CexecJobs.new("/bin/bash", {"-c","sysak memgraph"}, time, res.service_name)
        exec:addEvents(e)
        if isFile then
            local file = io.open(filepath, "rb")
            if file then
                local content = file:read("*a")
                file:close()
                local s = {
                    filename,
                    content,
                    "application/octet-stream"
                }
                self:postReqFile(s, res)
            else
                print("无法打开文件" .. filepath)
            end

        else
            local s = exec:readIn()
            self:postReq(s, res)
        end

    end
    if diag and diag.block then
        self._jobs[res.service_name] = diag.block
    else
        self._jobs[res.service_name] = 60
    end

end

function Cengine:pushTask(e, msgs)
    local events = pystring:split(msgs, '\n')
    for _, msg in ipairs(events) do
        local res = cjson.decode(msg)

        local service_name = res.service_name

        local cmd = res.cmd
        if service_name ~= nil then
            local diag = self._fYamlJobs[service_name]
            if self._jobs[service_name] then
                print(service_name .. " is blocking")
            else
                self:runJobs(e,res,diag)
            end

        elseif cmd == "mon_pid" then
            self._task:add(res.pid, res.loop)
        elseif cmd == "exec" then  -- exec a cmd
            local execCmd = res.exec
            local args = res.args or {}
            local second = res.second or 1
            local exec = CexecBase.new(execCmd, args, second)
            exec:addEvents(e)
        elseif cmd == "diag" and self._resDiag then
            local exec = res.exec
            local diag = self._resDiag[exec]
            if diag then
                system:dumps(diag)
                if self._diags[exec] then --实现阻塞
                    print("cmd " .. exec .. " is blocking.")
                else
                    self:run(e, res, diag)
                end
            end
        end
    end
end

function Cengine:proc(t, event, msgs)
    local lines = self._proto:protoTable()

    CvProto.proc(self, t)

    self:packLog("post_que", "post", cjson.encode(msgs))
    local bytes = self._proto:encode(lines)
    self._proto:que(bytes)

    self:pushTask(event, msgs)
end

function Cengine:checkDiag() --
    local toDel = {}
    for k, v in pairs(self._diags) do
        if v > 0 then
            self._diags[k] = v - 1
        else
            table.insert(toDel, k)
        end
    end
    for _, k in ipairs(toDel) do
        self._diags[k] = nil
    end
end

function Cengine:checkJobs() --
    local toDel = {}
    for k, v in pairs(self._jobs) do
        if v > 0 then
            self._jobs[k] = v - 1
        else
            table.insert(toDel, k)
        end
    end
    for _, k in ipairs(toDel) do
        self._jobs[k] = nil
    end
end

function Cengine:work(t, event)
    local msgs = postQue.pull()
    if msgs then
        self:proc(t, event, msgs)
    end
    self:checkDiag()
    if self._resDiag then
        self:checkJobs()
    end

end

return Cengine
