# -*- coding: utf-8 -*-
#!/root/anaconda3/envs/python310
import argparse
import json
import requests
import statistics
import datetime
import time
from db import get_sql_resp
from hum_byte_convert import hum_byte_convert, hum_convert
from utils import get_print_title_distance
from yaml_instance import load_resp_second_dist


config = load_resp_second_dist()
second_dist = config["config"]["freq"]

def traffic_data_show(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        ret = get_sql_resp(minutes=minutes, table=["networks"],date=date)
        distance_num = 0
        time_minute_flag = None
        minute_cpu_dict = {
                        "bytin":[],
                        "bytout":[],
                        "pktin":[],
                        "pktout":[],
                        "pkterr":[],
                        "pktdrp":[]
                    }
        cpu_dict_all={
                        "bytin":[],
                        "bytout":[],
                        "pktin":[],
                        "pktout":[],
                        "pkterr":[],
                        "pktdrp":[]
                    }
        title_traffic = "Time            bytin    bytout     pktin     pktout     pkterr     pktdrp"
        print("Time           ------------------------------traffic----------------------")
        print(title_traffic)
        title_print_distance_str = get_print_title_distance(title_traffic)
        endtime = datetime.datetime.fromtimestamp(int(ret[-1].get("time"))/1000000).strftime("%m/%d/%y-%H:%M")
        endtime_strp = datetime.datetime.strptime(endtime, "%m/%d/%y-%H:%M")
        for i in ret:
            time = i.get("time")
            time = datetime.datetime.fromtimestamp(int(time)/1000000).strftime("%m/%d/%y-%H:%M")
            time_strp = datetime.datetime.strptime(time, "%m/%d/%y-%H:%M")
            if (time_strp.minute+time_strp.hour*60)%distance_max == 1 and time_minute_flag != time and time_minute_flag != None:
                bytin = (sum(minute_cpu_dict.get("bytin")))*second_dist
                bytout = (sum(minute_cpu_dict.get("bytout")))*second_dist
                pktin = (sum(minute_cpu_dict.get("pktin")))*second_dist
                pktout = (sum(minute_cpu_dict.get("pktout")))*second_dist
                pkterr = (sum(minute_cpu_dict.get("pkterr")))*second_dist
                pktdrp = (sum(minute_cpu_dict.get("pktdrp")))*second_dist
                distance_num+=1
                if distance_num >=19:
                    print("Time           ------------------------------traffic----------------------")
                    print(title_traffic)
                    distance_num = 0
                if time_strp+datetime.timedelta(minutes=distance_max) > endtime_strp:
                    print(("%s\n"%title_print_distance_str).format(time_minute_flag,hum_byte_convert(bytin),hum_byte_convert(bytout), "%.2f"%pktin,"%.2f"%pktout, "%.2f"%pkterr, "%.2f"%pktdrp))
                    print(("%s"%title_print_distance_str).format("MAX",hum_byte_convert(max(cpu_dict_all.get("bytin"))),
                                                                        hum_byte_convert(max(cpu_dict_all.get("bytout"))), 
                                                                        "%.2f"%max(cpu_dict_all.get("pktin")),
                                                                        "%.2f"%max(cpu_dict_all.get("pktout")), 
                                                                        "%.2f"%max(cpu_dict_all.get("pkterr")),
                                                                        "%.2f"%max(cpu_dict_all.get("pktdrp"))
                                                                        ))
                    print(("%s"%title_print_distance_str).format("MEAN",hum_byte_convert(statistics.mean(cpu_dict_all.get("bytin"))),
                                                                        hum_byte_convert(statistics.mean(cpu_dict_all.get("bytout"))), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("pktin")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("pktout")), 
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("pkterr")),
                                                                        "%.2f"%statistics.mean(cpu_dict_all.get("pktdrp"))
                                                                        ))
                    print(("%s"%title_print_distance_str).format("MIN",hum_byte_convert(min(cpu_dict_all.get("bytin"))),
                                                                        hum_byte_convert(min(cpu_dict_all.get("bytout"))), 
                                                                        "%.2f"%min(cpu_dict_all.get("pktin")),
                                                                        "%.2f"%min(cpu_dict_all.get("pktout")),
                                                                        "%.2f"%min(cpu_dict_all.get("pkterr")),
                                                                        "%.2f"%min(cpu_dict_all.get("pktdrp"))
                                                                        ))
                    break
                else:
                    cpu_dict_all.get("bytin").append(bytin)
                    cpu_dict_all.get("bytout").append(bytout)
                    cpu_dict_all.get("pktin").append(pktin)
                    cpu_dict_all.get("pktout").append(pktout)
                    cpu_dict_all.get("pkterr").append(pkterr)
                    cpu_dict_all.get("pktdrp").append(pktdrp)
                    print(("%s"%title_print_distance_str).format(time_minute_flag,hum_byte_convert(bytin),hum_byte_convert(bytout), "%.2f"%pktin,"%.2f"%pktout, "%.2f"%pkterr, "%.2f"%pktdrp))
                    minute_cpu_dict = {
                            "bytin":[],
                            "bytout":[],
                            "pktin":[],
                            "pktout":[],
                            "pkterr":[],
                            "pktdrp":[]
                        }
                    if i.get("values").get("if_ibytes") !=None:
                        minute_cpu_dict.get("bytin").append(i.get("values").get("if_ibytes"))
                    if i.get("values").get("if_obytes") != None:
                        minute_cpu_dict.get("bytout").append(i.get("values").get("if_obytes"))
                    if i.get("values").get("if_ipackets") != None:
                        minute_cpu_dict.get("pktin").append(i.get("values").get("if_ipackets"))
                    if i.get("values").get("if_opackets") != None:
                        minute_cpu_dict.get("pktout").append(i.get("values").get("if_opackets"))
                    if i.get("values").get("if_oerrs") != None and i.get("values").get("if_ierrs")!= None:
                        minute_cpu_dict.get("pkterr").append((i.get("values").get("if_oerrs"))+(i.get("values").get("if_ierrs")))
                    if i.get("values").get("if_odrop") != None and i.get("values").get("if_idrop") != None:
                        minute_cpu_dict.get("pktdrp").append((i.get("values").get("if_odrop"))+(i.get("values").get("if_idrop")))
                    time_minute_flag = time
            else:
                if i.get("values").get("if_ibytes") !=None:
                    minute_cpu_dict.get("bytin").append(i.get("values").get("if_ibytes"))
                if i.get("values").get("if_obytes") != None:
                    minute_cpu_dict.get("bytout").append(i.get("values").get("if_obytes"))
                if i.get("values").get("if_ipackets") != None:
                    minute_cpu_dict.get("pktin").append(i.get("values").get("if_ipackets"))
                if i.get("values").get("if_opackets") != None:
                    minute_cpu_dict.get("pktout").append(i.get("values").get("if_opackets"))
                if i.get("values").get("if_oerrs") != None and i.get("values").get("if_ierrs")!= None:
                    minute_cpu_dict.get("pkterr").append((i.get("values").get("if_oerrs"))+(i.get("values").get("if_ierrs")))
                if i.get("values").get("if_odrop") != None and i.get("values").get("if_idrop") != None:
                    minute_cpu_dict.get("pktdrp").append((i.get("values").get("if_odrop"))+(i.get("values").get("if_idrop")))
                time_minute_flag = time
    except Exception as e:
        print(e)
        return