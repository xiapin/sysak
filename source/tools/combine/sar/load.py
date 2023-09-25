# -*- coding: utf-8 -*-
import statistics
import datetime
from db import get_sql_resp
from utils import get_print_title_distance


def load_data_show(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        ret = get_sql_resp(minutes=minutes, table=["proc_loadavg"],date=date)
        distance_num = 0
        time_minute_flag = None
        minute_cpu_dict = {
                        "load1":[],
                        "load5":[],
                        "load15":[],
                        "runq":[],
                        "plit":[]
                    }
        cpu_dict_all={
                        "load1":[],
                        "load5":[],
                        "load15":[],
                        "runq":[],
                        "plit":[]
                    }
        print("Time           -------------------load-----------------")
        print("Time            load1   load5  load15    runq    plit  ")
        title_load = "Time            load1   load5  load15    runq    plit  "
        title_print_distance_str = get_print_title_distance(title_load)
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
                        print("Time           -------------------load-----------------")
                        print("Time            load1   load5  load15    runq    plit")
                        distance_num = 0
                    minute_cpu_dict.get("load1").append(i.get("values").get("load1"))
                    minute_cpu_dict.get("load5").append(i.get("values").get("load5"))
                    minute_cpu_dict.get("load15").append(i.get("values").get("load15"))
                    minute_cpu_dict.get("runq").append(i.get("values").get("runq"))
                    minute_cpu_dict.get("plit").append(i.get("values").get("plit"))
                    load1 = (sum(minute_cpu_dict.get("load1"))/len(minute_cpu_dict.get("load1")))
                    load5 = (sum(minute_cpu_dict.get("load5"))/len(minute_cpu_dict.get("load5")))
                    load15 = (sum(minute_cpu_dict.get("load15"))/len(minute_cpu_dict.get("load15")))
                    runq = (sum(minute_cpu_dict.get("runq"))/len(minute_cpu_dict.get("runq")))
                    plit = (sum(minute_cpu_dict.get("plit"))/len(minute_cpu_dict.get("plit")))
                    cpu_dict_all.get("load1").append(load1)
                    cpu_dict_all.get("load5").append(load5)
                    cpu_dict_all.get("load15").append(load15)
                    cpu_dict_all.get("runq").append(runq)
                    cpu_dict_all.get("plit").append(plit)
                    print(("%s\n" %title_print_distance_str).format(time,"%.2f"%load1,"%.2f"%load5, "%.2f"%load15,"%.2f"%runq, "%.2f"%plit))
                    print(("%s"%title_print_distance_str).format("MAX","%.2f"%max(cpu_dict_all.get("load1")),"%.2f"%max(cpu_dict_all.get("load5")), 
                                                                        "%.2f"%max(cpu_dict_all.get("load15")),"%.2f"%max(cpu_dict_all.get("runq")), "%.2f"%max(cpu_dict_all.get("plit"))))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%statistics.mean(cpu_dict_all.get("load1")),"%.2f"%statistics.mean(cpu_dict_all.get("load5")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("load15")),"%.2f"%statistics.mean(cpu_dict_all.get("runq")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("plit"))))

                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("load1")),"%.2f"%min(cpu_dict_all.get("load5")), 
                                                                        "%.2f"%min(cpu_dict_all.get("load15")),"%.2f"%min(cpu_dict_all.get("runq")),
                                                                        "%.2f"%min(cpu_dict_all.get("plit"))))
                    break
                else:
                    load1 = (sum(minute_cpu_dict.get("load1"))/len(minute_cpu_dict.get("load1")))
                    load5 = (sum(minute_cpu_dict.get("load5"))/len(minute_cpu_dict.get("load5")))
                    load15 = (sum(minute_cpu_dict.get("load15"))/len(minute_cpu_dict.get("load15")))
                    runq = (sum(minute_cpu_dict.get("runq"))/len(minute_cpu_dict.get("runq")))
                    plit = (sum(minute_cpu_dict.get("plit"))/len(minute_cpu_dict.get("plit")))
                    cpu_dict_all.get("load1").append(load1)
                    cpu_dict_all.get("load5").append(load5)
                    cpu_dict_all.get("load15").append(load15)
                    cpu_dict_all.get("runq").append(runq)
                    cpu_dict_all.get("plit").append(plit)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -------------------load-----------------")
                        print("Time            load1   load5  load15    runq    plit")
                        distance_num = 0
                    print(("%s"%title_print_distance_str).format(time_minute_flag,"%.2f"%load1,"%.2f"%load5, "%.2f"%load15,"%.2f"%runq, "%.2f"%plit))
                    load1 = i.get("values").get("load1")
                    load5 = i.get("values").get("load5")
                    load15 = i.get("values").get("load15")
                    runq = i.get("values").get("runq")
                    plit = i.get("values").get("plit")
                    cpu_dict_all.get("load1").append(load1)
                    cpu_dict_all.get("load5").append(load5)
                    cpu_dict_all.get("load15").append(load15)
                    cpu_dict_all.get("runq").append(runq)
                    cpu_dict_all.get("plit").append(plit)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -------------------load-----------------")
                        print("Time            load1   load5  load15    runq    plit")
                        distance_num = 0
                    print(("%s\n"%title_print_distance_str).format(time,"%.2f"%load1,"%.2f"%load5, "%.2f"%load15,"%.2f"%runq, "%.2f"%plit))
                    print(("%s"%title_print_distance_str).format("MAX","%.2f"%max(cpu_dict_all.get("load1")),"%.2f"%max(cpu_dict_all.get("load5")), 
                                                                        "%.2f"%max(cpu_dict_all.get("load15")),"%.2f"%max(cpu_dict_all.get("runq")), "%.2f"%max(cpu_dict_all.get("plit"))))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%statistics.mean(cpu_dict_all.get("load1")),"%.2f"%statistics.mean(cpu_dict_all.get("load5")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("load15")),"%.2f"%statistics.mean(cpu_dict_all.get("runq")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("plit"))))

                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("load1")),"%.2f"%min(cpu_dict_all.get("load5")), 
                                                                        "%.2f"%min(cpu_dict_all.get("load15")),"%.2f"%min(cpu_dict_all.get("runq")),
                                                                        "%.2f"%min(cpu_dict_all.get("plit"))))
                    break
            if not time_minute_flag:
                minute_cpu_dict = {
                        "load1":[i.get("values").get("load1")],
                        "load5":[i.get("values").get("load5")],
                        "load15":[i.get("values").get("load15")],
                        "runq":[i.get("values").get("runq")],
                        "plit":[i.get("values").get("plit")]
                    }
                time_minute_flag = time
            elif time == time_minute_flag:
                minute_cpu_dict.get("load1").append(i.get("values").get("load1"))
                minute_cpu_dict.get("load5").append(i.get("values").get("load5"))
                minute_cpu_dict.get("load15").append(i.get("values").get("load15"))
                minute_cpu_dict.get("runq").append(i.get("values").get("runq"))
                minute_cpu_dict.get("plit").append(i.get("values").get("plit"))
            else:
                distance_num+=1
                if distance_num >=19:
                    print("Time           -------------------load-----------------")
                    print("Time            load1   load5  load15    runq    plit")
                    distance_num = 0
                load1 = (sum(minute_cpu_dict.get("load1"))/len(minute_cpu_dict.get("load1")))
                load5 = (sum(minute_cpu_dict.get("load5"))/len(minute_cpu_dict.get("load5")))
                load15 = (sum(minute_cpu_dict.get("load15"))/len(minute_cpu_dict.get("load15")))
                runq = (sum(minute_cpu_dict.get("runq"))/len(minute_cpu_dict.get("runq")))
                plit = (sum(minute_cpu_dict.get("plit"))/len(minute_cpu_dict.get("plit")))
                cpu_dict_all.get("load1").append(load1)
                cpu_dict_all.get("load5").append(load5)
                cpu_dict_all.get("load15").append(load15)
                cpu_dict_all.get("runq").append(runq)
                cpu_dict_all.get("plit").append(plit)
                print(("%s"%title_print_distance_str).format(time_minute_flag,"%.2f"%load1,"%.2f"%load5, "%.2f"%load15,"%.2f"%runq, "%.2f"%plit))
                minute_cpu_dict = {
                        "load1":[i.get("values").get("load1")],
                        "load5":[i.get("values").get("load5")],
                        "load15":[i.get("values").get("load15")],
                        "runq":[i.get("values").get("runq")],
                        "plit":[i.get("values").get("plit")]
                    }
                time_minute_flag = time
    except Exception as e:
        print(e)
        return