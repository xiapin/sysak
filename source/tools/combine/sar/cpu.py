# -*- coding: utf-8 -*-
#!/root/anaconda3/envs/python310
import statistics
import datetime

from db import get_sql_resp
from utils import get_print_title_distance

def cpu_data_show(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        ret = get_sql_resp(minutes=minutes, table=["cpu_total"],date=date)
        distance_num = 0
        time_minute_flag = None
        minute_cpu_dict = {
                        "user":[],
                        "sys":[],
                        "iowait":[],
                        "hardirq":[],
                        "softirq":[],
                        "steal":[],
                        "idle":[]
                    }
        cpu_dict_all={
                        "user":[],
                        "sys":[],
                        "iowait":[],
                        "hardirq":[],
                        "softirq":[],
                        "util":[]
                    }
        print("Time           -----------------------cpu----------------------")
        print("Time             user     sys    wait    hirq    sirq    util  ")
        title_cpu = "Time             user     sys    wait    hirq    sirq    util  "
        title_print_distance_str = get_print_title_distance(title_cpu)
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
                        print("Time           -----------------------cpu----------------------")
                        print("Time             user     sys    wait    hirq    sirq    util")
                        distance_num = 0
                    minute_cpu_dict.get("user").append(i.get("values").get("user"))
                    minute_cpu_dict.get("sys").append(i.get("values").get("sys"))
                    minute_cpu_dict.get("iowait").append(i.get("values").get("iowait"))
                    minute_cpu_dict.get("hardirq").append(i.get("values").get("hardirq"))
                    minute_cpu_dict.get("softirq").append(i.get("values").get("softirq"))
                    minute_cpu_dict.get("steal").append(i.get("values").get("steal"))
                    minute_cpu_dict.get("idle").append(i.get("values").get("idle"))
                    user = (sum(minute_cpu_dict.get("user"))/len(minute_cpu_dict.get("user")))
                    sys = (sum(minute_cpu_dict.get("sys"))/len(minute_cpu_dict.get("sys")))
                    iowait = (sum(minute_cpu_dict.get("iowait"))/len(minute_cpu_dict.get("iowait")))
                    hardirq = (sum(minute_cpu_dict.get("hardirq"))/len(minute_cpu_dict.get("hardirq")))
                    softirq = (sum(minute_cpu_dict.get("softirq"))/len(minute_cpu_dict.get("softirq")))
                    steal = (sum(minute_cpu_dict.get("steal"))/len(minute_cpu_dict.get("steal")))
                    idle = (sum(minute_cpu_dict.get("idle"))/len(minute_cpu_dict.get("idle")))
                    util = 100-idle-steal-iowait
                    cpu_dict_all.get("user").append(user)
                    cpu_dict_all.get("sys").append(sys)
                    cpu_dict_all.get("iowait").append(iowait)
                    cpu_dict_all.get("hardirq").append(hardirq)
                    cpu_dict_all.get("softirq").append(softirq)
                    cpu_dict_all.get("util").append(util)
                    print(("%s\n"%title_print_distance_str).format(time,"%.2f"%user,"%.2f"%sys, "%.2f"%iowait,"%.2f"%hardirq, "%.2f"%softirq,"%.2f"%util))
                    print(("%s"%title_print_distance_str).format("MAX","%.2f"%max(cpu_dict_all.get("user")),"%.2f"%max(cpu_dict_all.get("sys")), 
                                                                        "%.2f"%max(cpu_dict_all.get("iowait")),"%.2f"%max(cpu_dict_all.get("hardirq")), "%.2f"%max(cpu_dict_all.get("softirq")),
                                                                        "%.2f"%max(cpu_dict_all.get("util"))))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%statistics.mean(cpu_dict_all.get("user")),"%.2f"%statistics.mean(cpu_dict_all.get("sys")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("iowait")),"%.2f"%statistics.mean(cpu_dict_all.get("hardirq")), "%.2f"%statistics.mean(cpu_dict_all.get("softirq")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("util"))))

                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("user")),"%.2f"%min(cpu_dict_all.get("sys")), 
                                                                        "%.2f"%min(cpu_dict_all.get("iowait")),"%.2f"%min(cpu_dict_all.get("hardirq")), "%.2f"%min(cpu_dict_all.get("softirq")),
                                                                        "%.2f"%min(cpu_dict_all.get("util"))))
                    break
                else:
                    user = (sum(minute_cpu_dict.get("user"))/len(minute_cpu_dict.get("user")))
                    sys = (sum(minute_cpu_dict.get("sys"))/len(minute_cpu_dict.get("sys")))
                    iowait = (sum(minute_cpu_dict.get("iowait"))/len(minute_cpu_dict.get("iowait")))
                    hardirq = (sum(minute_cpu_dict.get("hardirq"))/len(minute_cpu_dict.get("hardirq")))
                    softirq = (sum(minute_cpu_dict.get("softirq"))/len(minute_cpu_dict.get("softirq")))
                    steal = (sum(minute_cpu_dict.get("steal"))/len(minute_cpu_dict.get("steal")))
                    idle = (sum(minute_cpu_dict.get("idle"))/len(minute_cpu_dict.get("idle")))
                    util = 100-idle-steal-iowait
                    cpu_dict_all.get("user").append(user)
                    cpu_dict_all.get("sys").append(sys)
                    cpu_dict_all.get("iowait").append(iowait)
                    cpu_dict_all.get("hardirq").append(hardirq)
                    cpu_dict_all.get("softirq").append(softirq)
                    cpu_dict_all.get("util").append(util)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -----------------------cpu----------------------")
                        print("Time             user     sys    wait    hirq    sirq    util")
                        distance_num = 0
                    print(("%s"%title_print_distance_str).format(time_minute_flag,"%.2f"%user,"%.2f"%sys, "%.2f"%iowait,"%.2f"%hardirq, "%.2f"%softirq,"%.2f"%util))
                    user = i.get("values").get("user")
                    sys = i.get("values").get("sys")
                    iowait = i.get("values").get("iowait")
                    hardirq = i.get("values").get("hardirq")
                    softirq = i.get("values").get("softirq")
                    steal = (sum(minute_cpu_dict.get("steal"))/len(minute_cpu_dict.get("steal")))
                    idle = (sum(minute_cpu_dict.get("idle"))/len(minute_cpu_dict.get("idle")))
                    util = 100-idle-steal-iowait
                    cpu_dict_all.get("user").append(user)
                    cpu_dict_all.get("sys").append(sys)
                    cpu_dict_all.get("iowait").append(iowait)
                    cpu_dict_all.get("hardirq").append(hardirq)
                    cpu_dict_all.get("softirq").append(softirq)
                    cpu_dict_all.get("util").append(util)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -----------------------cpu----------------------")
                        print("Time             user     sys    wait    hirq    sirq    util")
                        distance_num = 0
                    print(("%s\n"%title_print_distance_str).format(time,"%.2f"%user,"%.2f"%sys, "%.2f"%iowait,"%.2f"%hardirq, "%.2f"%softirq,"%.2f"%util))
                    print(("%s"%title_print_distance_str).format("MAX","%.2f"%max(cpu_dict_all.get("user")),"%.2f"%max(cpu_dict_all.get("sys")), 
                                                                        "%.2f"%max(cpu_dict_all.get("iowait")),"%.2f"%max(cpu_dict_all.get("hardirq")), "%.2f"%max(cpu_dict_all.get("softirq")),
                                                                        "%.2f"%max(cpu_dict_all.get("util"))))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%statistics.mean(cpu_dict_all.get("user")),"%.2f"%statistics.mean(cpu_dict_all.get("sys")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("iowait")),"%.2f"%statistics.mean(cpu_dict_all.get("hardirq")), "%.2f"%statistics.mean(cpu_dict_all.get("softirq")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("util"))))
                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("user")),"%.2f"%min(cpu_dict_all.get("sys")), 
                                                                        "%.2f"%min(cpu_dict_all.get("iowait")),"%.2f"%min(cpu_dict_all.get("hardirq")), "%.2f"%min(cpu_dict_all.get("softirq")),
                                                                        "%.2f"%min(cpu_dict_all.get("util"))))
                    break
            if not time_minute_flag:
                minute_cpu_dict = {
                        "user":[i.get("values").get("user")],
                        "sys":[i.get("values").get("sys")],
                        "iowait":[i.get("values").get("iowait")],
                        "hardirq":[i.get("values").get("hardirq")],
                        "softirq":[i.get("values").get("softirq")],
                        "steal":[i.get("values").get("steal")],
                        "idle":[i.get("values").get("idle")]
                    }
                time_minute_flag = time
            elif time == time_minute_flag:
                minute_cpu_dict.get("user").append(i.get("values").get("user"))
                minute_cpu_dict.get("sys").append(i.get("values").get("sys"))
                minute_cpu_dict.get("iowait").append(i.get("values").get("iowait"))
                minute_cpu_dict.get("hardirq").append(i.get("values").get("hardirq"))
                minute_cpu_dict.get("softirq").append(i.get("values").get("softirq"))
                minute_cpu_dict.get("steal").append(i.get("values").get("steal"))
                minute_cpu_dict.get("idle").append(i.get("values").get("idle"))
            else:
                distance_num+=1
                if distance_num >=19:
                    print("Time           -----------------------cpu----------------------")
                    print("Time             user     sys    wait    hirq    sirq    util")
                    distance_num = 0
                user = (sum(minute_cpu_dict.get("user"))/len(minute_cpu_dict.get("user")))
                sys = (sum(minute_cpu_dict.get("sys"))/len(minute_cpu_dict.get("sys")))
                iowait = (sum(minute_cpu_dict.get("iowait"))/len(minute_cpu_dict.get("iowait")))
                hardirq = (sum(minute_cpu_dict.get("hardirq"))/len(minute_cpu_dict.get("hardirq")))
                softirq = (sum(minute_cpu_dict.get("softirq"))/len(minute_cpu_dict.get("softirq")))
                steal = (sum(minute_cpu_dict.get("steal"))/len(minute_cpu_dict.get("steal")))
                idle = (sum(minute_cpu_dict.get("idle"))/len(minute_cpu_dict.get("idle")))
                util = 100-idle-steal-iowait
                cpu_dict_all.get("user").append(user)
                cpu_dict_all.get("sys").append(sys)
                cpu_dict_all.get("iowait").append(iowait)
                cpu_dict_all.get("hardirq").append(hardirq)
                cpu_dict_all.get("softirq").append(softirq)
                cpu_dict_all.get("util").append(util)
                print(("%s"%title_print_distance_str).format(time_minute_flag,"%.2f"%user,"%.2f"%sys, "%.2f"%iowait,"%.2f"%hardirq, "%.2f"%softirq,"%.2f"%util))
                minute_cpu_dict = {
                        "user":[i.get("values").get("user")],
                        "sys":[i.get("values").get("sys")],
                        "iowait":[i.get("values").get("iowait")],
                        "hardirq":[i.get("values").get("hardirq")],
                        "softirq":[i.get("values").get("softirq")],
                        "steal":[i.get("values").get("steal")],
                        "idle":[i.get("values").get("idle")]
                    }
                time_minute_flag = time
    except Exception as e:
        print(e)
        return