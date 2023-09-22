# -*- coding: utf-8 -*-
import statistics
import datetime

from db import get_sql_resp
from hum_byte_convert import hum_convert
from utils import get_print_title_distance

def mem_data_show(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        ret = get_sql_resp(minutes=minutes, table=["meminfo"], date=date)
        distance_num = 0
        time_minute_flag = None
        minute_cpu_dict = {
                        "free":[],
                        "used":[],
                        "buff":[],
                        "cach":[],
                        "total":[]
                    }
        cpu_dict_all={
                        "free":[],
                        "used":[],
                        "buff":[],
                        "cach":[],
                        "total":[],
                        "util":[]
                    }
        print("Time           -----------------------mem----------------------")
        print("Time             free    used    buff    cach   total    util  ")
        title_mem = "Time             free    used    buff    cach   total    util  "
        title_print_distance_str = get_print_title_distance(title_mem)
        endtime = datetime.datetime.fromtimestamp(int(ret[-1].get("time"))/1000000).strftime("%m/%d/%y-%H:%M")
        endtime_strp = datetime.datetime.strptime(endtime, "%m/%d/%y-%H:%M")
        for i in ret:
            time = i.get("time")
            time = datetime.datetime.fromtimestamp(int(time)/1000000).strftime("%m/%d/%y-%H:%M")
            time_strp = datetime.datetime.strptime(time, "%m/%d/%y-%H:%M")
            if (time_strp.minute+time_strp.hour*60)%distance_max != 0:
                continue
            if time_strp+datetime.timedelta(minutes=distance_max) >= endtime_strp:        #末条数据判断
                if time == time_minute_flag:   
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -----------------------mem----------------------")
                        print("Time             free    used    buff    cach   total    util  ")
                        distance_num = 0
                    minute_cpu_dict.get("free").append(i.get("values").get("MemFree"))
                    minute_cpu_dict.get("used").append(i.get("values").get("used"))
                    minute_cpu_dict.get("buff").append(i.get("values").get("user_buffers"))
                    minute_cpu_dict.get("cach").append(i.get("values").get("Cached"))
                    minute_cpu_dict.get("total").append(i.get("values").get("MemTotal"))
                    free = (sum(minute_cpu_dict.get("free"))/len(minute_cpu_dict.get("free")))
                    used = (sum(minute_cpu_dict.get("used"))/len(minute_cpu_dict.get("used")))
                    buff = (sum(minute_cpu_dict.get("buff"))/len(minute_cpu_dict.get("buff")))
                    cache = (sum(minute_cpu_dict.get("cach"))/len(minute_cpu_dict.get("cach")))
                    total = (sum(minute_cpu_dict.get("total"))/len(minute_cpu_dict.get("total")))
                    util = (total - free - buff - cache) / total * 100
                    cpu_dict_all.get("free").append(free)
                    cpu_dict_all.get("used").append(used)
                    cpu_dict_all.get("buff").append(buff)
                    cpu_dict_all.get("cach").append(cache)
                    cpu_dict_all.get("total").append(total)
                    cpu_dict_all.get("util").append(util)
                    print(("%s\n"%title_print_distance_str).format(time,hum_convert(free),hum_convert(used), hum_convert(buff),hum_convert(cache), hum_convert(total),hum_convert(util)))
                    print(("%s"%title_print_distance_str).format("MAX",hum_convert(max(cpu_dict_all.get("free"))),hum_convert(max(cpu_dict_all.get("used"))), 
                                                                        hum_convert(max(cpu_dict_all.get("buff"))),hum_convert(max(cpu_dict_all.get("cach"))), hum_convert(max(cpu_dict_all.get("total"))),
                                                                        hum_convert(max(cpu_dict_all.get("util")))))
                    print(("%s"%title_print_distance_str).format("MEAN",hum_convert(statistics.mean(cpu_dict_all.get("free"))),hum_convert(statistics.mean(cpu_dict_all.get("used"))), 
                                                                        hum_convert(statistics.mean(cpu_dict_all.get("buff"))),hum_convert(statistics.mean(cpu_dict_all.get("cach"))), 
                                                                        hum_convert(statistics.mean(cpu_dict_all.get("total"))),
                                                                        hum_convert(statistics.mean(cpu_dict_all.get("util")))))
                    print(("%s"%title_print_distance_str).format("MIN",hum_convert(min(cpu_dict_all.get("free"))),hum_convert(min(cpu_dict_all.get("used"))), 
                                                                        hum_convert(min(cpu_dict_all.get("buff"))),hum_convert(min(cpu_dict_all.get("cach"))), hum_convert(min(cpu_dict_all.get("total"))),
                                                                        hum_convert(min(cpu_dict_all.get("util")))))
                    break
                else:
                    free = (sum(minute_cpu_dict.get("free"))/len(minute_cpu_dict.get("free")))
                    used = (sum(minute_cpu_dict.get("used"))/len(minute_cpu_dict.get("used")))
                    buff = (sum(minute_cpu_dict.get("buff"))/len(minute_cpu_dict.get("buff")))
                    cache = (sum(minute_cpu_dict.get("cach"))/len(minute_cpu_dict.get("cach")))
                    total = (sum(minute_cpu_dict.get("total"))/len(minute_cpu_dict.get("total")))
                    util = (total - free - buff - cache) / total * 100
                    cpu_dict_all.get("free").append(free)
                    cpu_dict_all.get("used").append(used)
                    cpu_dict_all.get("buff").append(buff)
                    cpu_dict_all.get("cach").append(cache)
                    cpu_dict_all.get("total").append(total)
                    cpu_dict_all.get("util").append(util)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -----------------------mem----------------------")
                        print("Time             free    used    buff    cach   total    util")
                        distance_num = 0
                    print(("%s"%title_print_distance_str).format(time_minute_flag,hum_convert(free),hum_convert(used), hum_convert(buff),hum_convert(cache), hum_convert(total),hum_convert(util)))
                    free = i.get("values").get("MemFree")
                    used = i.get("values").get("used")
                    buff = i.get("values").get("user_buffers")
                    cache = i.get("values").get("Cached")
                    total = i.get("values").get("MemTotal")
                    util = (total - free - buff - cache) / total * 100
                    cpu_dict_all.get("free").append(free)
                    cpu_dict_all.get("used").append(used)
                    cpu_dict_all.get("buff").append(buff)
                    cpu_dict_all.get("cach").append(cache)
                    cpu_dict_all.get("total").append(total)
                    cpu_dict_all.get("util").append(util)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -----------------------mem----------------------")
                        print("Time             free    used    buff    cach   total    util")
                        distance_num = 0
                    print(("%s\n"%title_print_distance_str).format(time,hum_convert(free),hum_convert(used), hum_convert(buff),hum_convert(cache), hum_convert(total),hum_convert(util)))
                    print(("%s"%title_print_distance_str).format("MAX",hum_convert(max(cpu_dict_all.get("free"))),hum_convert(max(cpu_dict_all.get("used"))), 
                                                                        hum_convert(max(cpu_dict_all.get("buff"))),hum_convert(max(cpu_dict_all.get("cach"))), hum_convert(max(cpu_dict_all.get("total"))),
                                                                        hum_convert(max(cpu_dict_all.get("util")))))
                    print(("%s"%title_print_distance_str).format("MEAN",hum_convert(statistics.mean(cpu_dict_all.get("free"))),hum_convert(statistics.mean(cpu_dict_all.get("used"))), 
                                                                        hum_convert(statistics.mean(cpu_dict_all.get("buff"))),hum_convert(statistics.mean(cpu_dict_all.get("cach"))), 
                                                                        hum_convert(statistics.mean(cpu_dict_all.get("total"))),
                                                                        hum_convert(statistics.mean(cpu_dict_all.get("util")))))
                    print(("%s"%title_print_distance_str).format("MIN",hum_convert(min(cpu_dict_all.get("free"))),hum_convert(min(cpu_dict_all.get("used"))), 
                                                                        hum_convert(min(cpu_dict_all.get("buff"))),hum_convert(min(cpu_dict_all.get("cach"))), hum_convert(min(cpu_dict_all.get("total"))),
                                                                        hum_convert(min(cpu_dict_all.get("util")))))
                    break
            if not time_minute_flag:
                minute_cpu_dict = {
                        "free":[i.get("values").get("MemFree")],
                        "used":[i.get("values").get("used")],
                        "buff":[i.get("values").get("user_buffers")],
                        "cach":[i.get("values").get("Cached")],
                        "total":[i.get("values").get("MemTotal")]
                    }
                time_minute_flag = time
            elif time == time_minute_flag:
                minute_cpu_dict.get("free").append(i.get("values").get("MemFree"))
                minute_cpu_dict.get("used").append(i.get("values").get("used"))
                minute_cpu_dict.get("buff").append(i.get("values").get("user_buffers"))
                minute_cpu_dict.get("cach").append(i.get("values").get("Cached"))
                minute_cpu_dict.get("total").append(i.get("values").get("MemTotal"))
            else:
                free = i.get("values").get("MemFree")
                used = i.get("values").get("used")
                buff = i.get("values").get("user_buffers")
                cache = i.get("values").get("Cached")
                total = i.get("values").get("MemTotal")
                util = (total - free - buff - cache) / total * 100
                cpu_dict_all.get("free").append(free)
                cpu_dict_all.get("used").append(used)
                cpu_dict_all.get("buff").append(buff)
                cpu_dict_all.get("cach").append(cache)
                cpu_dict_all.get("total").append(total)
                cpu_dict_all.get("util").append(util)
                print(("%s"%title_print_distance_str).format(time_minute_flag,hum_convert(free),hum_convert(used), hum_convert(buff),hum_convert(cache), hum_convert(total),hum_convert(util)))
                distance_num+=1
                if distance_num >=19:
                    print("Time           -----------------------mem----------------------")
                    print("Time             free    used    buff    cach   total    util")
                    distance_num = 0
                minute_cpu_dict = {
                        "free":[i.get("values").get("MemFree")],
                        "used":[i.get("values").get("used")],
                        "buff":[i.get("values").get("user_buffers")],
                        "cach":[i.get("values").get("Cached")],
                        "total":[i.get("values").get("MemTotal")]
                    }
                time_minute_flag = time
    except Exception as e:
        print(e)
        return