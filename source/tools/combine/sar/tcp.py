# -*- coding: utf-8 -*-
import statistics
import datetime
from db import get_sql_resp
from utils import get_print_title_distance

def tcp_data_show(distance_max, minutes, date):
    try:
        if not distance_max:
            distance_max = 5
        ret = get_sql_resp(minutes=minutes, table=["sock_stat"],date=date)
        res_tcp = get_sql_resp(minutes=minutes, table=["net_tcp_count"],date=date)
        distance_num = 0
        time_minute_flag = None
        minute_cpu_dict = {
                        "active":[],
                        "pasive":[],
                        "iseg":[],
                        "outseg":[],
                        "curres":[],
                        "retranssegs":[]
                    }
        cpu_dict_all={
                        "active":[],
                        "pasive":[],
                        "iseg":[],
                        "outseg":[],
                        "curres":[],
                        "retran":[]
                    }
        print("Time           ---------------------tcp----------------------")
        title_tcp = "Time           active  pasive    iseg  outseg  CurrEs  retran"
        print(title_tcp)
        title_print_distance_str = get_print_title_distance(title_tcp)
        endtime = datetime.datetime.fromtimestamp(int(ret[-1].get("time"))/1000000).strftime("%m/%d/%y-%H:%M")
        endtime_strp = datetime.datetime.strptime(endtime, "%m/%d/%y-%H:%M")
        for i in ret:
            time = i.get("time")
            time = datetime.datetime.fromtimestamp(int(time)/1000000).strftime("%m/%d/%y-%H:%M")
            time_strp = datetime.datetime.strptime(time, "%m/%d/%y-%H:%M")
            if (time_strp.minute+time_strp.hour*60)%distance_max != 0:
                continue
            index_i = ret.index(i)
            if time_strp+datetime.timedelta(minutes=distance_max) >= endtime_strp:        #末条数据判断
                if time == time_minute_flag:   
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           ---------------------tcp----------------------")
                        print("Time           active  pasive    iseg  outseg  CurrEs  retran")
                        distance_num = 0
                    minute_cpu_dict.get("active").append(i.get("values").get("tcp_inuse"))
                    minute_cpu_dict.get("pasive").append(i.get("values").get("tcp_tw"))
                    minute_cpu_dict.get("iseg").append(res_tcp[index_i].get("values").get("InSegs"))
                    minute_cpu_dict.get("outseg").append(res_tcp[index_i].get("values").get("OutSegs"))
                    minute_cpu_dict.get("curres").append(res_tcp[index_i].get("values").get("CurrEstab"))
                    minute_cpu_dict.get("retranssegs").append(res_tcp[index_i].get("values").get("RetransSegs"))
                    active = (sum(minute_cpu_dict.get("active"))/len(minute_cpu_dict.get("active")))
                    pasive = (sum(minute_cpu_dict.get("pasive"))/len(minute_cpu_dict.get("pasive")))
                    iseg = (sum(minute_cpu_dict.get("iseg"))/len(minute_cpu_dict.get("iseg")))
                    outseg = (sum(minute_cpu_dict.get("outseg"))/len(minute_cpu_dict.get("outseg")))
                    curres = (sum(minute_cpu_dict.get("curres"))/len(minute_cpu_dict.get("curres")))
                    retran = (sum(minute_cpu_dict.get("retranssegs"))/sum(minute_cpu_dict.get("outseg")))
                    cpu_dict_all.get("active").append(active)
                    cpu_dict_all.get("pasive").append(pasive)
                    cpu_dict_all.get("iseg").append(iseg)
                    cpu_dict_all.get("outseg").append(outseg)
                    cpu_dict_all.get("curres").append(curres)
                    cpu_dict_all.get("retran").append(retran)
                    print(("%s\n"%title_print_distance_str).format(time, "%.2f"%active, "%.2f"%pasive, "%.2f"%iseg,"%.2f"%outseg, "%.2f"%curres, "%.2f"%retran))
                    print(("%s"%title_print_distance_str).format("MAX", "%.2f"%max(cpu_dict_all.get("active")),
                                                                        "%.2f"%max(cpu_dict_all.get("pasive")), 
                                                                        "%.2f"%max(cpu_dict_all.get("iseg")),
                                                                        "%.2f"%max(cpu_dict_all.get("outseg")), 
                                                                        "%.2f"%max(cpu_dict_all.get("curres")),
                                                                        "%.2f"%max(cpu_dict_all.get("retran"))
                                                                        ))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%(statistics.mean(cpu_dict_all.get("active"))),
                                                                        "%.2f"%(statistics.mean(cpu_dict_all.get("pasive"))), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("iseg")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("outseg")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("curres")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("retran"))
                                                                        ))
                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("active")),
                                                                        "%.2f"%min(cpu_dict_all.get("pasive")), 
                                                                        "%.2f"%min(cpu_dict_all.get("iseg")),
                                                                        "%.2f"%min(cpu_dict_all.get("outseg")),
                                                                        "%.2f"%min(cpu_dict_all.get("curres")),
                                                                        "%.2f"%min(cpu_dict_all.get("retran"))
                                                                        ))
                    break
                else:
                    active = (sum(minute_cpu_dict.get("active"))/len(minute_cpu_dict.get("active")))
                    pasive = (sum(minute_cpu_dict.get("pasive"))/len(minute_cpu_dict.get("pasive")))
                    iseg = (sum(minute_cpu_dict.get("iseg"))/len(minute_cpu_dict.get("iseg")))
                    outseg = sum(minute_cpu_dict.get("outseg"))
                    curres = (sum(minute_cpu_dict.get("curres"))/len(minute_cpu_dict.get("curres")))
                    retran = (sum(minute_cpu_dict.get("retranssegs"))/sum(minute_cpu_dict.get("outseg")))
                    cpu_dict_all.get("active").append(active)
                    cpu_dict_all.get("pasive").append(pasive)
                    cpu_dict_all.get("iseg").append(iseg)
                    cpu_dict_all.get("outseg").append(outseg)
                    cpu_dict_all.get("curres").append(curres)
                    cpu_dict_all.get("retran").append(retran)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           ---------------------tcp----------------------")
                        print("Time           active  pasive    iseg  outseg  CurrEs  retran")
                        distance_num = 0
                    print(("%s"%title_print_distance_str).format(time_minute_flag, "%.2f"%active, "%.2f"%pasive, "%.2f"%iseg,"%.2f"%outseg, "%.2f"%curres, "%.2f"%retran))
                    active = i.get("values").get("tcp_inuse")
                    pasive = i.get("values").get("tcp_tw")
                    iseg = res_tcp[index_i].get("values").get("InSegs")
                    outseg = res_tcp[index_i].get("values").get("OutSegs")
                    curres = res_tcp[index_i].get("values").get("CurrEstab")
                    retran = (res_tcp[index_i].get("values").get("RetransSegs"))/(res_tcp[index_i].get("values").get("OutSegs"))
                    cpu_dict_all.get("active").append(active)
                    cpu_dict_all.get("pasive").append(pasive)
                    cpu_dict_all.get("iseg").append(iseg)
                    cpu_dict_all.get("outseg").append(outseg)
                    cpu_dict_all.get("curres").append(curres)
                    cpu_dict_all.get("retran").append(retran)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           ---------------------tcp----------------------")
                        print("Time           active  pasive    iseg  outseg  CurrEs  retran")
                        distance_num = 0
                    print(("%s\n"%title_print_distance_str).format(time, "%.2f"%active, "%.2f"%pasive, "%.2f"%iseg,"%.2f"%outseg, "%.2f"%curres, "%.2f"%retran))
                    print(("%s"%title_print_distance_str).format("MAX", "%.2f"%max(cpu_dict_all.get("active")),
                                                                        "%.2f"%max(cpu_dict_all.get("pasive")), 
                                                                        "%.2f"%max(cpu_dict_all.get("iseg")),
                                                                        "%.2f"%max(cpu_dict_all.get("outseg")), 
                                                                        "%.2f"%max(cpu_dict_all.get("curres")),
                                                                        "%.2f"%max(cpu_dict_all.get("retran"))
                                                                        ))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%(statistics.mean(cpu_dict_all.get("active"))),
                                                                        "%.2f"%(statistics.mean(cpu_dict_all.get("pasive"))), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("iseg")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("outseg")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("curres")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("retran"))
                                                                        ))
                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("active")),
                                                                        "%.2f"%min(cpu_dict_all.get("pasive")), 
                                                                        "%.2f"%min(cpu_dict_all.get("iseg")),
                                                                        "%.2f"%min(cpu_dict_all.get("outseg")),
                                                                        "%.2f"%min(cpu_dict_all.get("curres")),
                                                                        "%.2f"%min(cpu_dict_all.get("retran"))
                                                                        ))
                    break
            if not time_minute_flag:
                if i.get("values").get("tcp_inuse") != None:
                    minute_cpu_dict.get("active").append(i.get("values").get("tcp_inuse"))
                if i.get("values").get("tcp_tw") != None:
                    minute_cpu_dict.get("pasive").append(i.get("values").get("tcp_tw"))
                if res_tcp[index_i].get("values").get("InSegs") != None:
                    minute_cpu_dict.get("iseg").append(res_tcp[index_i].get("values").get("InSegs"))
                if res_tcp[index_i].get("values").get("OutSegs") != None:
                    minute_cpu_dict.get("outseg").append(res_tcp[index_i].get("values").get("OutSegs"))
                if res_tcp[index_i].get("values").get("CurrEstab") != None:
                    minute_cpu_dict.get("curres").append(res_tcp[index_i].get("values").get("CurrEstab"))
                if res_tcp[index_i].get("values").get("RetransSegs") != None:
                    minute_cpu_dict.get("retranssegs").append(res_tcp[index_i].get("values").get("RetransSegs"))
                time_minute_flag = time
            elif time == time_minute_flag:
                if i.get("values").get("tcp_inuse") != None:
                    minute_cpu_dict.get("active").append(i.get("values").get("tcp_inuse"))
                if i.get("values").get("tcp_tw") != None:
                    minute_cpu_dict.get("pasive").append(i.get("values").get("tcp_tw"))
                if res_tcp[index_i].get("values").get("InSegs") != None:
                    minute_cpu_dict.get("iseg").append(res_tcp[index_i].get("values").get("InSegs"))
                if res_tcp[index_i].get("values").get("OutSegs") != None:
                    minute_cpu_dict.get("outseg").append(res_tcp[index_i].get("values").get("OutSegs"))
                if res_tcp[index_i].get("values").get("CurrEstab") != None:
                    minute_cpu_dict.get("curres").append(res_tcp[index_i].get("values").get("CurrEstab"))
                if res_tcp[index_i].get("values").get("RetransSegs") != None:
                    minute_cpu_dict.get("retranssegs").append(res_tcp[index_i].get("values").get("RetransSegs"))
            else:
                distance_num+=1
                if distance_num >=19:
                    print("Time           ---------------------tcp----------------------")
                    print("Time           active  pasive    iseg  outseg  CurrEs  retran")
                    distance_num = 0
                if i.get("values").get("tcp_inuse") != None:
                    minute_cpu_dict.get("active").append(i.get("values").get("tcp_inuse"))
                if i.get("values").get("tcp_tw") != None:
                    minute_cpu_dict.get("pasive").append(i.get("values").get("tcp_tw"))
                if res_tcp[index_i].get("values").get("InSegs") != None:
                    minute_cpu_dict.get("iseg").append(res_tcp[index_i].get("values").get("InSegs"))
                if res_tcp[index_i].get("values").get("OutSegs") != None:
                    minute_cpu_dict.get("outseg").append(res_tcp[index_i].get("values").get("OutSegs"))
                if res_tcp[index_i].get("values").get("CurrEstab") != None:
                    minute_cpu_dict.get("curres").append(res_tcp[index_i].get("values").get("CurrEstab"))
                if res_tcp[index_i].get("values").get("RetransSegs") != None:
                    minute_cpu_dict.get("retranssegs").append(res_tcp[index_i].get("values").get("RetransSegs"))
                active = (sum(minute_cpu_dict.get("active"))/len(minute_cpu_dict.get("active")))
                pasive = (sum(minute_cpu_dict.get("pasive"))/len(minute_cpu_dict.get("pasive")))
                iseg = (sum(minute_cpu_dict.get("iseg"))/len(minute_cpu_dict.get("iseg")))
                outseg = (sum(minute_cpu_dict.get("outseg"))/len(minute_cpu_dict.get("outseg")))
                curres = (sum(minute_cpu_dict.get("curres"))/len(minute_cpu_dict.get("curres")))
                retran = (sum(minute_cpu_dict.get("retranssegs"))/len(minute_cpu_dict.get("retranssegs")))
                print(("%s"%title_print_distance_str).format(time_minute_flag, "%.2f"%active, "%.2f"%pasive, "%.2f"%iseg,"%.2f"%outseg, "%.2f"%curres, "%.2f"%retran))
                minute_cpu_dict = {
                        "active":[],
                        "pasive":[],
                        "iseg":[],
                        "outseg":[],
                        "curres":[],
                        "retranssegs":[]
                    }
                if i.get("values").get("tcp_inuse") != None:
                    minute_cpu_dict.get("active").append(i.get("values").get("tcp_inuse"))
                if i.get("values").get("tcp_tw") != None:
                    minute_cpu_dict.get("pasive").append(i.get("values").get("tcp_tw"))
                if res_tcp[index_i].get("values").get("InSegs") != None:
                    minute_cpu_dict.get("iseg").append(res_tcp[index_i].get("values").get("InSegs"))
                if res_tcp[index_i].get("values").get("OutSegs") != None:
                    minute_cpu_dict.get("outseg").append(res_tcp[index_i].get("values").get("OutSegs"))
                if res_tcp[index_i].get("values").get("CurrEstab") != None:
                    minute_cpu_dict.get("curres").append(res_tcp[index_i].get("values").get("CurrEstab"))
                if res_tcp[index_i].get("values").get("RetransSegs") != None:
                    minute_cpu_dict.get("retranssegs").append(res_tcp[index_i].get("values").get("RetransSegs"))
                time_minute_flag = time
    except Exception as e:
        print(e)
        return