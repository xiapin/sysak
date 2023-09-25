# -*- coding: utf-8 -*-


import statistics
import datetime

from db import get_sql_resp
from utils import get_print_title_distance


def pcsw_data_show(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        ret = get_sql_resp(minutes=minutes, table=["stat_counters"], date=date)
        distance_num = 0
        time_minute_flag = None
        minute_cpu_dict = {
                        "block":[],
                        "ctxt":[],
                        "run":[]
                    }
        cpu_dict_all= {
                        "block":[],
                        "ctxt":[],
                        "run":[]
                    }
        print("Time------------------pcsw----------------")
        print("Time             block      ctxt       run")
        title_pcsw = "Time             block      ctxt       run"
        title_print_distance_str = get_print_title_distance(title_pcsw)
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
                        print("Time------------------pcsw----------------")
                        print("Time             block      ctxt       run")
                        distance_num = 0
                    minute_cpu_dict.get("block").append(i.get("values").get("procs_blocked"))
                    minute_cpu_dict.get("ctxt").append(i.get("values").get("ctxt"))
                    minute_cpu_dict.get("run").append(i.get("values").get("procs_running"))
                    procs_blocked = (sum(minute_cpu_dict.get("block"))/len(minute_cpu_dict.get("block")))
                    ctxt = (sum(minute_cpu_dict.get("ctxt"))/len(minute_cpu_dict.get("ctxt")))
                    procs_running = (sum(minute_cpu_dict.get("run"))/len(minute_cpu_dict.get("run")))
                    cpu_dict_all.get("block").append(procs_blocked)
                    cpu_dict_all.get("run").append(procs_running)              
                    print(("%s\n"%title_print_distance_str).format(time,"%.2f"%procs_blocked,"%.2f"%ctxt,"%.2f"%procs_running))
                    print(("%s"%title_print_distance_str).format("MAX","%.2f"%max(cpu_dict_all.get("block")),
                                                                        "%.2f"%max(cpu_dict_all.get("ctxt")),
                                                                        "%.2f"%max(cpu_dict_all.get("run"))))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%statistics.mean(cpu_dict_all.get("block")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("ctxt")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("run"))))
                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("block")),
                                                                        "%.2f"%min(cpu_dict_all.get("ctxt")),
                                                                        "%.2f"%min(cpu_dict_all.get("run"))))
                    break
                else:
                    procs_blocked = (sum(minute_cpu_dict.get("block"))/len(minute_cpu_dict.get("block")))
                    ctxt = (sum(minute_cpu_dict.get("ctxt"))/len(minute_cpu_dict.get("ctxt")))
                    procs_running = (sum(minute_cpu_dict.get("run"))/len(minute_cpu_dict.get("run")))
                    cpu_dict_all.get("block").append(procs_blocked)
                    cpu_dict_all.get("run").append(procs_running)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time------------------pcsw----------------")
                        print("Time             block      ctxt       run")
                        distance_num = 0
                    print(("%s"%title_print_distance_str).format(time_minute_flag,"%.2f"%procs_blocked,"%.2f"%ctxt,"%.2f"%procs_running))
                    procs_blocked = i.get("values").get("procs_blocked")
                    ctxt = i.get("values").get("ctxt")
                    procs_running = i.get("values").get("procs_running")
                    cpu_dict_all.get("block").append(procs_blocked)
                    cpu_dict_all.get("ctxt").append(ctxt)
                    cpu_dict_all.get("run").append(procs_running)
                    distance_num+=1
                    if distance_num >=distance_max:
                        print("Time------------------pcsw----------------")
                        print("Time             block      ctxt       run")
                        distance_num = 0
                    print(("%s\n"%title_print_distance_str).format(time,"%.2f"%procs_blocked,"%.2f"%ctxt,"%.2f"%procs_running))
                    print(("%s"%title_print_distance_str).format("MAX","%.2f"%max(cpu_dict_all.get("block")),
                                                                        "%.2f"%max(cpu_dict_all.get("ctxt")),
                                                                        "%.2f"%max(cpu_dict_all.get("run"))))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%statistics.mean(cpu_dict_all.get("block")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("ctxt")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("run"))))
                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("block")),
                                                                        "%.2f"%min(cpu_dict_all.get("ctxt")),
                                                                        "%.2f"%min(cpu_dict_all.get("run"))))
                    break
            if not time_minute_flag:
                if i.get("values").get("procs_blocked") != None:
                    minute_cpu_dict.get("block").append(i.get("values").get("procs_blocked"))
                if i.get("values").get("ctxt") != None:
                    minute_cpu_dict.get("ctxt").append(i.get("values").get("ctxt"))
                if i.get("values").get("procs_running") != None:
                    minute_cpu_dict.get("run").append(i.get("values").get("procs_running"))
                time_minute_flag = time
            elif time == time_minute_flag:
                if i.get("values").get("procs_blocked") != None:
                    minute_cpu_dict.get("block").append(i.get("values").get("procs_blocked"))
                if i.get("values").get("ctxt") != None:
                    minute_cpu_dict.get("ctxt").append(i.get("values").get("ctxt"))
                if i.get("values").get("procs_running") != None:
                    minute_cpu_dict.get("run").append(i.get("values").get("procs_running"))
            else:
                distance_num+=1
                if distance_num >=19:
                    print("Time------------------pcsw----------------")
                    print("Time             block      ctxt       run")
                    distance_num = 0
                procs_blocked = (sum(minute_cpu_dict.get("block"))/len(minute_cpu_dict.get("block")))
                ctxt = (sum(minute_cpu_dict.get("ctxt"))/len(minute_cpu_dict.get("ctxt")))
                procs_running = (sum(minute_cpu_dict.get("run"))/len(minute_cpu_dict.get("run")))
                cpu_dict_all.get("block").append(procs_blocked)
                cpu_dict_all.get("ctxt").append(ctxt)
                cpu_dict_all.get("run").append(procs_running)
                print(("%s"%title_print_distance_str).format(time_minute_flag,"%.2f"%procs_blocked,"%.2f"%ctxt,"%.2f"%procs_running))
                minute_cpu_dict = {
                        "block":[],
                        "ctxt":[],
                        "run":[]
                    }
                if i.get("values").get("procs_blocked") != None:
                    minute_cpu_dict.get("block").append(i.get("values").get("procs_blocked"))
                if i.get("values").get("ctxt") != None:
                    minute_cpu_dict.get("ctxt").append(i.get("values").get("ctxt"))
                if i.get("values").get("procs_running") != None:
                    minute_cpu_dict.get("run").append(i.get("values").get("procs_running"))
                time_minute_flag = time
    except Exception as e:
        print(e)
        return