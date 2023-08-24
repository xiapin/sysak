---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by liaozhaoyan.
--- DateTime: 2022/12/17 11:04 AM
---

require("common.class")

local system = require("common.system")
local snappy = require("snappy")
local pystring = require("common.pystring")
local CprotoData = require("common.protoData")
local foxFFI = require("tsdb.native.foxffi")
local dirent = require("posix.dirent")
local unistd = require("posix.unistd")
local cjson = require("cjson.safe")
local inotify = require('inotify')

local CfoxTSDB = class("CfoxTSDB")

local json = cjson.new()
function CfoxTSDB:_init_(fYaml)
    self.ffi = foxFFI.ffi
    self.cffi = foxFFI.cffi
    self._proto = CprotoData.new(nil)
    self:setupConf(fYaml)
end

function CfoxTSDB:_del_()

    if self._man then
        self.cffi.fox_del_man(self._man)
        self._man = nil
    end

    if self._ino then
        for _, w in ipairs(self._ino_ws) do
            self._ino:rmwatch(w)
        end
        self._ino:close()
        self._ino = nil
    end
end

function CfoxTSDB:_initInotify()
    self._ino = inotify.init()
    system:fdNonBlocking(self._ino:getfd())
    self._ino_ws = {}
end

function CfoxTSDB:setupConf(fYaml)
    local conf = system:parseYaml(fYaml)
    local dbConf =  conf.config.db or {budget = 200, rotate=7}
    self._qBudget = dbConf.budget or 200
    self._rotate = dbConf.rotate or 7
end

function CfoxTSDB:fileNames()
    if self._man then
        local iname = self.ffi.string(self._man.iname)
        local fname = self.ffi.string(self._man.fname)
        return {iname = iname,
                fname = fname}
    end
end

function CfoxTSDB:get_us()
    return self.cffi.get_us()
end

function CfoxTSDB:getDateFrom_us(us)
    local foxTime = self.ffi.new("struct foxDate")

    assert(self.cffi.get_date_from_us(us, foxTime) == 0)

    return foxTime
end

function CfoxTSDB:getDate()
    local foxTime = self.ffi.new("struct foxDate")

    assert(self.cffi.get_date(foxTime) == 0)

    return foxTime
end

function CfoxTSDB:makeStamp(foxTime)
    return self.cffi.make_stamp(foxTime)
end

function CfoxTSDB:date2str(date)
    local d = string.format("%04d-%02d-%02d", date.year + 1900, date.mon + 1, date.mday)
    local t = string.format("%02d:%02d:%02d", date.hour, date.min, date.sec)
    return d .. " " .. t
end

local function transDate(ds)
    local year, mon, mday = unpack(ds)
    return tonumber(year) - 1900, tonumber(mon) - 1, tonumber(mday)
end

local function transTime(ts)
    local hour, min, sec = unpack(ts)
    return tonumber(hour), tonumber(min), tonumber(sec)
end

function CfoxTSDB:str2date(s)
    local dt = pystring:split(s, " ", 1)
    local d, t = dt[1], dt[2]

    local ds = pystring:split(d, "-", 2)
    local ts = pystring:split(t, ":", 2)

    local foxTime = self.ffi.new("struct foxDate")
    foxTime.year, foxTime.mon, foxTime.mday = transDate(ds)
    foxTime.hour, foxTime.min, foxTime.sec  = transTime(ts)

    return foxTime
end

function CfoxTSDB:deltaSec(date)
    local delta = 0

    if date.sec then
        delta = delta + date.sec
    end
    if date.min then
        delta = delta + date.min * 60
    end
    if date.hour then
        delta = delta + date.hour * 60 * 60
    end
    if date.day then
        delta = delta + date.day * 24 * 60 * 60
    end
    return delta
end

function CfoxTSDB:moveSec(foxTime, off_sec)
    local us = self:makeStamp(foxTime) + off_sec * 1e6
    return self:getDateFrom_us(us)
end

function CfoxTSDB:movesSec(s, off_sec)
    local foxTime = self:str2date(s)
    return self:date2str(self:moveSec(foxTime, off_sec))
end

function CfoxTSDB:moveTime(foxTime, tTable)
    local sec = self:deltaSec(tTable)
    return self:moveSec(foxTime, sec)
end

function CfoxTSDB:movesTime(s, tTable)
    local foxTime = self:str2date(s)
    return self:date2str(self:moveTime(foxTime, tTable))
end

function CfoxTSDB:packLine(lines)
    return self._proto:encode(lines)
end

function CfoxTSDB:rotateDb()
    local usec = self._man.now
    local sec = self._rotate * 24 * 60 * 60

    local foxTime = self:getDateFrom_us(usec - sec * 1e6)
    local level = foxTime.year * 10000 + foxTime.mon * 100 + foxTime.mday

    local ok, files = pcall(dirent.files, './')
    if not ok then
        return
    end

    for f in files do
        if string.match(f,"^%d%d%d%d%d%d%d%d%.fox$") then
            local sf = string.sub(f, 1, 8)
            local num = tonumber(sf)
            if num < level then
                print("delete " .. "./" .. f)
                pcall(unistd.unlink, "./" .. f) --delete
            end
        end
    end
end

function CfoxTSDB:setupWrite()
    assert(self._man == nil, "one fox object should have only one manager.")
    self._man = self.ffi.new("struct fox_manager")
    local date = self:getDate()
    local us = self:get_us()
    local ret = self.cffi.fox_setup_write(self._man, date, us)

    if ret ~= 0 then
        local usec = self:get_us()

        local foxTime = self:getDateFrom_us(usec)
        local v = foxTime.year * 10000 + foxTime.mon * 100 + foxTime.mday
        local fname = string.format("%08d.fox", v)
        pcall(unistd.unlink, "./" .. fname)
        ret = self.cffi.fox_setup_write(self._man, date, us)
    end
    assert(ret == 0)
    return ret
end

local function getTables(proto, buff)
    local tables = {}

    local lines = proto:decode(buff)
    for _, line in ipairs(lines.lines) do
        local title = line.line
        if tables[title] then
            tables[title] = tables[title] + 1
        else
            tables[title] = 1
        end
    end
    return tables
end

function CfoxTSDB:_write(tables, buff)
    assert(self._man ~= nil, "this fox object show setup for read or write, you should call setupWrite after new")
    local now = self:get_us()
    local date = self:getDateFrom_us(now)
    local tableStream = snappy.compress(json.encode(tables))
    local stream = snappy.compress(buff)

    local tableLen = #tableStream
    local streamLen = #stream

    assert(self.cffi.fox_write(self._man, date, now,
                    self.ffi.string(tableStream, tableLen), tableLen,
                    self.ffi.string(stream, streamLen), streamLen) > 0 )

    if self._man.new_day > 0 then
        self:rotateDb()
    end
end

function CfoxTSDB:write(buff)
    local tables = getTables(self._proto, buff)
    if system:keyCount(tables) then
        self:_write(tables, buff)
    end
end

local function addWatch(ino, ws, fname)
    local w = ino:addwatch(fname, inotify.IN_MODIFY)
    table.insert(ws, w)
end

local function dbIsUpdate(ino)
    local ret = false
    for _ in ino:events() do
        ret = true
    end
    return ret
end

function CfoxTSDB:_setupRead(us)
    assert(self._man == nil, "one fox object should have only one manager.")
    self._man = self.ffi.new("struct fox_manager")
    us = us or (self:get_us() - 1)
    local date = self:getDateFrom_us(us)
    local res = self.cffi.fox_setup_read(self._man, date, us)
    assert(res >= 0, string.format("setup read return %d.", res))
    if res > 0 then
        self.cffi.fox_del_man(self._man)
        self._man = nil
    else
        self:_initInotify()
        local names = self:fileNames()
        addWatch(self._ino, self._ino_ws, names.iname)
    end


    return res
end

function CfoxTSDB:curMove(us)
    assert(self._man)
    local ret = self.cffi.fox_cur_move(self._man, us)
    assert(ret >= 0, string.format("cur move bad ret: %d", ret))
    return self._man.r_index
end

function CfoxTSDB:_resize()
    if dbIsUpdate(self._ino) then
        local ret = self.cffi.fox_read_resize(self._man)
        assert(ret >= 0, string.format("resize bad ret: %d", ret))
    end
end

function CfoxTSDB:resize()
    print("warn: resize function is no need any more.")
end

local function loadFoxTable(ffi, cffi, pman)
    local tables = {}

    local data = ffi.new("char* [1]")
    local ret  = cffi.fox_read_table(pman, data)
    assert(ret >= 0)
    if ret > 0 then
        local stream = ffi.string(data[0], ret)
        local ustr = snappy.decompress(stream)
        tables = json.decode(ustr)
        cffi.fox_free_buffer(data)
    end
    return tables
end

local function loadFoxData(ffi, cffi, pman, proto)
    local lines = {}

    local data = ffi.new("char* [1]")
    local us = ffi.new("fox_time_t [1]")
    local ret  = cffi.fox_read_data(pman, data, us)
    assert(ret >= 0)
    if ret > 0 then
        local stream = ffi.string(data[0], ret)
        local ustr = snappy.decompress(stream)
        lines = proto:decode(ustr)
        lines['time'] = tonumber(us[0])
        cffi.fox_free_buffer(data)
    end

    return lines
end

local function checkQTable(qtbl, ffi, cffi, pman)
    if qtbl == nil then  -- nil means get all
        return true
    end

    local tables = loadFoxTable(ffi, cffi, pman)
    for _, tbl in ipairs(qtbl) do
        if tables[tbl] then
            return true
        end
    end
    return false
end

local function transLine(line, time, addLog)
    local cell = {time = time, title = line.line}

    local labels = {}
    if line.ls then
        for _, vlabel in ipairs(line.ls) do
            labels[vlabel.name] = vlabel.index
        end
    end
    cell.labels = labels

    local values = {}
    if line.vs then
        for _, vvalue in ipairs(line.vs) do
            values[vvalue.name] = vvalue.value
        end
    end
    cell.values = values

    local logs = {}
    if addLog then
        if line.log then
            for _, log in ipairs(line.log) do
                logs[log.name] = log.log
            end
        end
    end
    cell.logs = logs

    return cell
end

local function filterLines(qtbl, lines, cells, addLog)
    local c = #cells + 1
    if not lines.lines then
        return
    end
    local time = lines.time

    if qtbl == nil then  -- nil means get all
        for _, line in ipairs(lines.lines) do
            cells[c] = transLine(line, time, addLog)
            c = c + 1
        end
        return
    end

    for _, tbl in ipairs(qtbl) do
        for _, line in ipairs(lines.lines) do
            if line.line == tbl then
                cells[c] = transLine(line, time, addLog)
                c = c + 1
            end
        end
    end
    return cells
end

-- stop_us: end time; qtbl: query tables, if nil, get all; addLog, if nil only get values,
function CfoxTSDB:loadData(stop_us, qtbl, addLog)
    local stop = false

    local function fLoad()
        local cells = {}

        if stop then
            return nil
        end

        if checkQTable(qtbl, self.ffi, self.cffi, self._man) then
            local datas = loadFoxData(self.ffi, self.cffi, self._man, self._proto)
            filterLines(qtbl, datas, cells, addLog)
        end

        local ret = self.cffi.fox_next(self._man, stop_us)
        assert(ret >= 0, "for next failed.")
        if ret > 0 then
            stop = true
        end
        return cells
    end
    return fLoad
end

function CfoxTSDB:loadTable(stop_us)
    local stop = false

    local function fTable()
        if stop then
            return nil
        end

        local tables = loadFoxTable(self.ffi, self.cffi, self._man)

        local ret = self.cffi.fox_next(self._man, stop_us)
        assert(ret >= 0, "for next failed.")
        if ret > 0 then
            stop = true
        end

        return tables
    end
    return fTable
end


function CfoxTSDB:_loadMetric(start, stop, ms)  -- start stop should at the same mday
    assert(stop > start)
    local dStart = self:getDateFrom_us(start)
    local dStop = self:getDateFrom_us(stop)

    assert(self.cffi.check_foxdate(dStart, dStop) == 1)  -- check date
    assert(self._man)

    self:curMove(start)    -- moveto position

    local lenMs = #ms + 1
    for cells in self:loadData(stop, nil, false) do  -- only for metric, do not need log.
        for _, cell in ipairs(cells) do
            ms[lenMs] = cell
            lenMs = lenMs + 1
        end
    end
    return ms
end

function CfoxTSDB:_preQuery(date, beg)
    if not self._man then    -- check _man is already installed.
        if self:_setupRead(beg) ~= 0 then    -- try to create new
            return 1    -- beg is  nil
        end
        return 0
    end

    if self.cffi.check_pman_date(self._man, date) == 1 then
        self:_resize()  -- check is need to resize.
        return 0 -- at the same day
    else
        self:_del_()  -- to destroy..
        if self:_setupRead(beg) ~= 0 then    -- try to setup new
            return 1
        end
        return 0
    end
end

function CfoxTSDB:_qLastMetric(date, beg, stop, ms)
    local res = self:_preQuery(date, beg)

    if res > 0 then
        return ms
    elseif res == 0 then
        return self:_loadMetric(beg, stop, ms)
    else
        error("bad preQuery state: " .. res)
    end
end

function CfoxTSDB:qLastMetric(last, ms)
    assert(last < 24 * 60 * 60)

    local now = self:get_us()
    local beg = now - last * 1e6

    local dStart = self:getDateFrom_us(beg)
    local dStop = self:getDateFrom_us(now)

    if self.cffi.check_foxdate(dStart, dStop) ~= 0 then
        self:_qLastMetric(dStart, beg, now, ms)
    else
        dStop.hour, dStop.min, dStop.sec = 0, 0, 0
        local beg1 = beg
        local beg2 = self.cffi.make_stamp(dStop)
        local now1 = beg2 - 1
        local now2 = now

        self:_qLastMetric(dStart, beg1, now1, ms)
        self:_qLastMetric(dStop,  beg2, now2, ms)
    end
end

function CfoxTSDB:qDay(date, start, stop, ms, tbls, budget)
    local ret = self:_preQuery(date, start)
    if ret ~= 0 then
        return ms
    end

    budget = budget or self._qBudget
    self:curMove(start)
    local inc = false
    local lenMs = #ms + 1
    for cells in self:loadData(stop, tbls, true) do
        inc = false
        for _, cell in ipairs(cells) do
            inc = true
            ms[lenMs] = cell
            lenMs = lenMs + 1
        end

        if inc then
            budget = budget - 1
        end
        if budget == 0 then   -- max len
            break
        end
    end
    return ms
end

function CfoxTSDB:qDayTables(date, start, stop, tbls)
    local ret = self:_preQuery(date, start)
    if ret ~= 0 then
        return tbls
    end

    self:curMove(start)
    for cells in self:loadTable(stop) do
        for k, v in pairs(cells) do
            if tbls[k] then
                tbls[k] = tbls[k] + v
            else
                tbls[k] = v
            end
        end
    end
    return tbls
end

function CfoxTSDB:qDate(dStart, dStop, tbls)
    local now = self:makeStamp(dStop)
    local beg = self:makeStamp(dStart)

    if now - beg > 24 * 60 * 60 * 1e6 then
        return {}
    end

    local ms = {}
    if self.cffi.check_foxdate(dStart, dStop) ~= 0 then
        self:qDay(dStart, beg, now, ms, tbls)
    else
        dStop.hour, dStop.min, dStop.sec = 0, 0, 0
        local beg1 = beg
        local beg2 = self.cffi.make_stamp(dStop)
        local now1 = beg2 - 1
        local now2 = now

        self:qDay(dStart, beg1, now1, ms, tbls)
        local budget = self._qBudget - #ms
        if budget > 0 then
            self:qDay(dStop, beg2, now2, ms, tbls, budget)
        end
    end
    return ms
end

function CfoxTSDB:qNow(sec, tbls)
    if sec > 24 * 60 * 60 then
        return {}
    end
    local now = self:get_us()
    local beg = now - sec * 1e6 + 1

    local dStart = self:getDateFrom_us(beg)
    local dStop = self:getDateFrom_us(now)

    local ms = {}
    if self.cffi.check_foxdate(dStart, dStop) ~= 0 then
        self:qDay(dStart, beg, now, ms, tbls)
    else
        dStop.hour, dStop.min, dStop.sec = 0, 0, 0
        local beg1 = beg
        local beg2 = self.cffi.make_stamp(dStop)
        local now1 = beg2 - 1
        local now2 = now

        self:qDay(dStart, beg1, now1, ms, tbls)
        local budget = self._qBudget - #ms
        if budget > 0 then
            self:qDay(dStop, beg2, now2, ms, tbls, budget)
        end
    end
    return ms
end

function CfoxTSDB:qTabelNow(sec)
    if sec > 24 * 60 * 60 then
        return {}
    end
    local now = self:get_us()
    local beg = now - sec * 1e6 + 1

    local dStart = self:getDateFrom_us(beg)
    local dStop = self:getDateFrom_us(now)

    local tbls = {}
    if self.cffi.check_foxdate(dStart, dStop) ~= 0 then
        self:qDayTables(dStart, beg, now, tbls)
    else
        dStop.hour, dStop.min, dStop.sec = 0, 0, 0
        local beg1 = beg
        local beg2 = self.cffi.make_stamp(dStop)
        local now1 = beg2 - 1
        local now2 = now

        self:qDayTables(dStart, beg1, now1, tbls)
        self:qDayTables(dStop,  beg2, now2, tbls)
    end

    local res = {}
    local c = 1
    for k, _ in pairs(tbls) do
        res[c] = k
        c = c + 1
    end
    return res
end

return CfoxTSDB
