# -*- coding: utf-8 -*-
import statistics
import datetime
from db import get_sql_resp
from utils import get_print_title_distance

def udp_data_show(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        ret = get_sql_resp(minutes=minutes, table=["net_udp_count"],date=date)
        distance_num = 0
        time_minute_flag = None
        minute_cpu_dict = {
                        "InErrors":[],
                        "SndbufErrors":[],
                        "InDatagrams":[],
                        "RcvbufErrors":[],
                        "OutDatagrams":[],
                        "NoPorts":[]
                    }
        cpu_dict_all= {
                        "InErrors":[],
                        "SndbufErrors":[],      
                        "InDatagrams":[],
                        "RcvbufErrors":[],
                        "OutDatagrams":[],
                        "NoPorts":[]
                    }
        print("Time           -----------------------udp-----------------------------------------")
        print("Time             InEr     SndEr       In         RcvEr        Out           NoPort")
        title_udp = ("Time             InEr     SndEr       InDa       RcvEr        OutDa         NoPort")
        title_print_distance_str = get_print_title_distance(title_udp)
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
                        print("Time           -----------------------udp-----------------------------------------")
                        print("Time             InEr     SndEr       In         RcvEr        Out           NoPort")
                        distance_num = 0
                    minute_cpu_dict.get("InErrors").append(i.get("values").get("InErrors"))
                    minute_cpu_dict.get("SndbufErrors").append(i.get("values").get("SndbufErrors"))
                    minute_cpu_dict.get("InDatagrams").append(i.get("values").get("InDatagrams"))
                    minute_cpu_dict.get("RcvbufErrors").append(i.get("values").get("RcvbufErrors"))
                    minute_cpu_dict.get("OutDatagrams").append(i.get("values").get("OutDatagrams"))
                    minute_cpu_dict.get("NoPorts").append(i.get("values").get("NoPorts"))
                    InErrors = (sum(minute_cpu_dict.get("InErrors"))/len(minute_cpu_dict.get("InErrors")))
                    SndbufErrors = (sum(minute_cpu_dict.get("SndbufErrors"))/len(minute_cpu_dict.get("SndbufErrors")))
                    InDatagrams = (sum(minute_cpu_dict.get("InDatagrams"))/len(minute_cpu_dict.get("InDatagrams")))
                    RcvbufErrors = (sum(minute_cpu_dict.get("RcvbufErrors"))/len(minute_cpu_dict.get("RcvbufErrors")))
                    OutDatagrams = (sum(minute_cpu_dict.get("OutDatagrams"))/len(minute_cpu_dict.get("OutDatagrams")))
                    NoPorts = (sum(minute_cpu_dict.get("NoPorts"))/len(minute_cpu_dict.get("NoPorts")))
                    cpu_dict_all.get("InErrors").append(InErrors)
                    cpu_dict_all.get("SndbufErrors").append(SndbufErrors)
                    cpu_dict_all.get("InDatagrams").append(InDatagrams)
                    cpu_dict_all.get("RcvbufErrors").append(RcvbufErrors)
                    cpu_dict_all.get("OutDatagrams").append(OutDatagrams)
                    cpu_dict_all.get("NoPorts").append(NoPorts)                
                    print(("%s\n"%title_print_distance_str).format(time,"%.2f"%InErrors,"%.2f"%SndbufErrors, "%.2f"%InDatagrams,"%.2f"%RcvbufErrors, "%.2f"%OutDatagrams,"%.2f"%NoPorts))
                    print(("%s\n"%title_print_distance_str).format("MAX","%.2f"%max(cpu_dict_all.get("InErrors")),
                                                                        "%.2f"%max(cpu_dict_all.get("SndbufErrors")), 
                                                                        "%.2f"%max(cpu_dict_all.get("InDatagrams")),
                                                                        "%.2f"%max(cpu_dict_all.get("RcvbufErrors")), 
                                                                        "%.2f"%max(cpu_dict_all.get("OutDatagrams")),
                                                                        "%.2f"%max(cpu_dict_all.get("NoPorts"))))
                    print(("%s\n"%title_print_distance_str).format("MEAN","%.2f"%statistics.mean(cpu_dict_all.get("InErrors")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("SndbufErrors")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("InDatagrams")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("RcvbufErrors")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("OutDatagrams")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("NoPorts"))))

                    print(("%s\n"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("InErrors")),
                                                                        "%.2f"%min(cpu_dict_all.get("SndbufErrors")), 
                                                                        "%.2f"%min(cpu_dict_all.get("InDatagrams")),
                                                                        "%.2f"%min(cpu_dict_all.get("RcvbufErrors")), 
                                                                        "%.2f"%min(cpu_dict_all.get("OutDatagrams")),
                                                                        "%.2f"%min(cpu_dict_all.get("NoPorts"))))
                    break
                else:
                    InErrors = (sum(minute_cpu_dict.get("InErrors"))/len(minute_cpu_dict.get("InErrors")))
                    SndbufErrors = (sum(minute_cpu_dict.get("SndbufErrors"))/len(minute_cpu_dict.get("SndbufErrors")))
                    InDatagrams = (sum(minute_cpu_dict.get("InDatagrams"))/len(minute_cpu_dict.get("InDatagrams")))
                    RcvbufErrors = (sum(minute_cpu_dict.get("RcvbufErrors"))/len(minute_cpu_dict.get("RcvbufErrors")))
                    OutDatagrams = (sum(minute_cpu_dict.get("OutDatagrams"))/len(minute_cpu_dict.get("OutDatagrams")))
                    NoPorts = (sum(minute_cpu_dict.get("NoPorts"))/len(minute_cpu_dict.get("NoPorts")))
                    cpu_dict_all.get("InErrors").append(InErrors)
                    cpu_dict_all.get("SndbufErrors").append(SndbufErrors)
                    cpu_dict_all.get("InDatagrams").append(InDatagrams)
                    cpu_dict_all.get("RcvbufErrors").append(RcvbufErrors)
                    cpu_dict_all.get("OutDatagrams").append(OutDatagrams)
                    cpu_dict_all.get("NoPorts").append(NoPorts)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -----------------------udp-----------------------------------------")
                        print("Time             InEr     SndEr       In         RcvEr        Out           NoPort")
                        distance_num = 0
                    print(("%s"%title_print_distance_str).format(time_minute_flag,"%.2f"%InErrors,"%.2f"%SndbufErrors, "%.2f"%InDatagrams,"%.2f"%RcvbufErrors, "%.2f"%OutDatagrams,"%.2f"%NoPorts))
                    InErrors = i.get("values").get("InErrors")
                    SndbufErrors = i.get("values").get("SndbufErrors")
                    InDatagrams = i.get("values").get("InDatagrams")
                    RcvbufErrors = i.get("values").get("RcvbufErrors")
                    OutDatagrams = i.get("values").get("OutDatagrams")
                    NoPorts = i.get("values").get("NoPorts")
                    cpu_dict_all.get("InErrors").append(InErrors)
                    cpu_dict_all.get("SndbufErrors").append(SndbufErrors)
                    cpu_dict_all.get("InDatagrams").append(InDatagrams)
                    cpu_dict_all.get("RcvbufErrors").append(RcvbufErrors)
                    cpu_dict_all.get("OutDatagrams").append(OutDatagrams)
                    cpu_dict_all.get("NoPorts").append(NoPorts)
                    distance_num+=1
                    if distance_num >=19:
                        print("Time           -----------------------udp-----------------------------------------")
                        print("Time             InEr     SndEr       In         RcvEr        Out           NoPort")
                        distance_num = 0
                    print(("%s\n"%title_print_distance_str).format(time,"%.2f"%InErrors,"%.2f"%SndbufErrors, "%.2f"%InDatagrams,"%.2f"%RcvbufErrors, "%.2f"%OutDatagrams,"%.2f"%NoPorts))
                    print(("%s"%title_print_distance_str).format("MAX","%.2f"%max(cpu_dict_all.get("InErrors")),
                                                                        "%.2f"%max(cpu_dict_all.get("SndbufErrors")), 
                                                                        "%.2f"%max(cpu_dict_all.get("InDatagrams")),
                                                                        "%.2f"%max(cpu_dict_all.get("RcvbufErrors")), 
                                                                        "%.2f"%max(cpu_dict_all.get("OutDatagrams")),
                                                                        "%.2f"%max(cpu_dict_all.get("NoPorts"))))
                    print(("%s"%title_print_distance_str).format("MEAN","%.2f"%statistics.mean(cpu_dict_all.get("InErrors")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("SndbufErrors")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("InDatagrams")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("RcvbufErrors")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("OutDatagrams")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("NoPorts"))))

                    print(("%s"%title_print_distance_str).format("MIN","%.2f"%min(cpu_dict_all.get("InErrors")),
                                                                        "%.2f"%min(cpu_dict_all.get("SndbufErrors")), 
                                                                        "%.2f"%min(cpu_dict_all.get("InDatagrams")),
                                                                        "%.2f"%min(cpu_dict_all.get("RcvbufErrors")), 
                                                                        "%.2f"%min(cpu_dict_all.get("OutDatagrams")),
                                                                        "%.2f"%min(cpu_dict_all.get("NoPorts"))))
                    break
            if not time_minute_flag:
                minute_cpu_dict = {
                        "InErrors":[i.get("values").get("InErrors")],
                        "SndbufErrors":[i.get("values").get("SndbufErrors")],
                        "InDatagrams":[i.get("values").get("InDatagrams")],
                        "RcvbufErrors":[i.get("values").get("RcvbufErrors")],
                        "OutDatagrams":[i.get("values").get("OutDatagrams")],
                        "NoPorts":[i.get("values").get("NoPorts")]
                    }
                time_minute_flag = time
            elif time == time_minute_flag:
                minute_cpu_dict.get("InErrors").append(i.get("values").get("InErrors"))
                minute_cpu_dict.get("SndbufErrors").append(i.get("values").get("SndbufErrors"))
                minute_cpu_dict.get("InDatagrams").append(i.get("values").get("InDatagrams"))
                minute_cpu_dict.get("RcvbufErrors").append(i.get("values").get("RcvbufErrors"))
                minute_cpu_dict.get("OutDatagrams").append(i.get("values").get("OutDatagrams"))
                minute_cpu_dict.get("NoPorts").append(i.get("values").get("NoPorts"))
            else:
                distance_num+=1
                if distance_num >=19:
                    print("Time           -----------------------udp-----------------------------------------")
                    print("Time             InEr     SndEr       In         RcvEr        Out           NoPort")
                    distance_num = 0
                InErrors = (sum(minute_cpu_dict.get("InErrors"))/len(minute_cpu_dict.get("InErrors")))
                SndbufErrors = (sum(minute_cpu_dict.get("SndbufErrors"))/len(minute_cpu_dict.get("SndbufErrors")))
                InDatagrams = (sum(minute_cpu_dict.get("InDatagrams"))/len(minute_cpu_dict.get("InDatagrams")))
                RcvbufErrors = (sum(minute_cpu_dict.get("RcvbufErrors"))/len(minute_cpu_dict.get("RcvbufErrors")))
                OutDatagrams = (sum(minute_cpu_dict.get("OutDatagrams"))/len(minute_cpu_dict.get("OutDatagrams")))
                NoPorts = (sum(minute_cpu_dict.get("NoPorts"))/len(minute_cpu_dict.get("NoPorts")))
                cpu_dict_all.get("InErrors").append(InErrors)
                cpu_dict_all.get("SndbufErrors").append(SndbufErrors)
                cpu_dict_all.get("InDatagrams").append(InDatagrams)
                cpu_dict_all.get("RcvbufErrors").append(RcvbufErrors)
                cpu_dict_all.get("OutDatagrams").append(OutDatagrams)
                cpu_dict_all.get("NoPorts").append(NoPorts)
                print(("%s"%title_print_distance_str).format(time_minute_flag,"%.2f"%InErrors,"%.2f"%SndbufErrors, "%.2f"%InDatagrams,"%.2f"%RcvbufErrors, "%.2f"%OutDatagrams,"%.2f"%NoPorts))
                minute_cpu_dict = {
                        "InErrors":[i.get("values").get("InErrors")],
                        "SndbufErrors":[i.get("values").get("SndbufErrors")],
                        "InDatagrams":[i.get("values").get("InDatagrams")],
                        "RcvbufErrors":[i.get("values").get("RcvbufErrors")],
                        "OutDatagrams":[i.get("values").get("OutDatagrams")],
                        "NoPorts":[i.get("values").get("NoPorts")]
                    }
                time_minute_flag = time
    except Exception as e:
        print(e)
        return