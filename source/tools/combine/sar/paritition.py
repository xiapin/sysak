# -*- coding: utf-8 -*-

import statistics
import datetime
from db import get_sql_resp
from hum_byte_convert import hum_byte_convert
from utils import get_print_title_distance

def partition_data_show(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        ret = get_sql_resp(minutes=minutes, table=["fs_stat"], date=date)
        if not ret:
            return
        
        print("Time          --------------------paritition-----------------------------------")
        print("Time             path                   bfree       bused      btotal      util")
        #     "Time             path                   f_bfree     f_bsize    f_blocks    util"
        title_partition = "Time             path                   f_bfree     f_bsize    f_blocks    util"
        title_print_distance_str = get_print_title_distance(title_partition)
        title_list = title_partition.split(' ')
        title_list = [val for val in title_list if val]
        minute_dict, dict_all, distance_num, time_minute_flag = get_cpu_dict(ret, distance_max, title_list, title_print_distance_str)
        if minute_dict:
            distance_num+=1
            if distance_num >=10:
                print("Time          --------------------paritition-----------------------------------")
                print("Time             path                   bfree       bused      btotal      util")
                distance_num = 0
            for k in minute_dict.keys():
                try:
                    f_bfree = (sum(minute_dict.get(k).get("f_bfree", 0))/len(minute_dict.get(k).get("f_bfree")))
                    f_bsize = (sum(minute_dict.get(k).get("f_bsize"))/len(minute_dict.get(k).get("f_bsize")))
                    f_blocks = (sum(minute_dict.get(k).get("f_blocks"))/len(minute_dict.get(k).get("f_blocks")))
                except:
                    f_bfree = 0
                    f_bsize = 0
                    f_blocks = 0
                bfree = f_bfree*f_bsize
                bused = (f_blocks-f_bfree)*f_bsize
                btotal= f_blocks * f_bsize
                if btotal == 0:
                    util = 0
                else:
                    util = bused/btotal
                if k not in dict_all.keys():
                    dict_all[k] = {}
                if not dict_all.get(k).get("bfree"):
                    dict_all[k]["bfree"]= [bfree]
                else:
                    dict_all.get(k).get("bfree").append(bfree)
                if not dict_all.get(k).get("bused"):
                    dict_all[k]["bused"]= [bused]
                else:
                    dict_all.get(k).get("bused").append(bused) 
                if not dict_all.get(k).get("btotal"):
                    dict_all[k]["btotal"]= [btotal]
                else:
                    dict_all.get(k).get("btotal").append(btotal)
                if not dict_all.get(k).get("util"):
                        dict_all[k]["util"]= [util]
                else:
                    dict_all.get(k).get("util").append(util)
                print(("%s"%title_print_distance_str).format(time_minute_flag, k , hum_byte_convert(bfree),hum_byte_convert(bused), hum_byte_convert(btotal),"%.2f"%util))
        if not dict_all:
            return
        for k in dict_all.keys():
            print(("\n%s"%title_print_distance_str).format("MAX",k ,hum_byte_convert(max(dict_all.get(k).get("bfree"))),
                                                                    hum_byte_convert(max(dict_all.get(k).get("bused"))), 
                                                                    hum_byte_convert(max(dict_all.get(k).get("btotal"))),
                                                                    "%.2f"%(max(dict_all.get(k).get("util")))))
            print(("%s"%title_print_distance_str).format("MEAN", k , hum_byte_convert(statistics.mean(dict_all.get(k).get("bfree"))),
                                                                    hum_byte_convert(statistics.mean(dict_all.get(k).get("bused"))),                                                     
                                                                    hum_byte_convert(statistics.mean(dict_all.get(k).get("btotal"))),
                                                                    "%.2f"%(statistics.mean(dict_all.get(k).get("util")))))
            print(("%s"%title_print_distance_str).format("MIN", k , hum_byte_convert(min(dict_all.get(k).get("bfree"))),
                                                                    hum_byte_convert(min(dict_all.get(k).get("bused"))), 
                                                                    hum_byte_convert(min(dict_all.get(k).get("btotal"))),
                                                                    "%.2f"%(min(dict_all.get(k).get("util")))))
    except Exception as e:
        print(e)
        return
        
def get_cpu_dict(ret, distance_max, title_list, title_print_distance_str):
    time_minute_flag = None
    minute_dict = {}
    dict_all = {}
    distance_num = 0
    try:
        for i in ret:
            time = i.get("time")
            time = datetime.datetime.fromtimestamp(int(time)/1000000).strftime("%m/%d/%y-%H:%M")
            time_strp = datetime.datetime.strptime(time, "%m/%d/%y-%H:%M")
            if (time_strp.minute+time_strp.hour*60)%distance_max != 0:
                continue
            if time == time_minute_flag or time_minute_flag == None:
                disk_name = i.get("labels").get("mount")
                disk_name = disk_name.split("/dev/")[-1]
                for k in title_list[2:5]:
                    if disk_name not in minute_dict.keys():
                        minute_dict[disk_name] = {}
                    elif k not in minute_dict.get(disk_name).keys():
                        if i.get("values").get(k) != None: 
                            minute_dict[disk_name][k] = [i.get("values").get(k)]
                    else:
                        minute_dict.get(disk_name).get(k).append(i.get("values").get(k))
                    if disk_name not in dict_all.keys():
                        dict_all[disk_name] = {}
                time_minute_flag=time
            else:
                distance_num+=1
                if distance_num >=10:
                    print("Time          --------------------paritition-----------------------------------")
                    print("Time             path                   bfree       bused      btotal      util")
                    distance_num = 0
                for k in minute_dict.keys():
                    try:
                        f_bfree = (sum(minute_dict.get(k).get("f_bfree", 0))/len(minute_dict.get(k).get("f_bfree")))
                        f_bsize = (sum(minute_dict.get(k).get("f_bsize"))/len(minute_dict.get(k).get("f_bsize")))
                        f_blocks = (sum(minute_dict.get(k).get("f_blocks"))/len(minute_dict.get(k).get("f_blocks")))
                    except:
                        f_bfree = 0
                        f_bsize = 0
                        f_blocks = 0
                    bfree = f_bfree*f_bsize
                    bused = (f_blocks-f_bfree)*f_bsize
                    btotal= f_blocks * f_bsize
                    util = bused/btotal
                    if k not in dict_all.keys():
                        dict_all[k] = {}
                    if not dict_all.get(k).get("bfree"):
                        dict_all[k]["bfree"]= [bfree]
                    else:
                        dict_all.get(k).get("bfree").append(bfree)

                    if not dict_all.get(k).get("bused"):
                        dict_all[k]["bused"]= [bused]
                    else:
                        dict_all.get(k).get("bused").append(bused) 
                    if not dict_all.get(k).get("btotal"):
                        dict_all[k]["btotal"]= [btotal]
                    else:
                        dict_all.get(k).get("btotal").append(btotal)
                    if not dict_all.get(k).get("util"):
                            dict_all[k]["util"]= [util]
                    else:
                        dict_all.get(k).get("util").append(util)
                    print(("%s"%title_print_distance_str).format(time_minute_flag, k , hum_byte_convert(bfree),hum_byte_convert(bused), hum_byte_convert(btotal),"%.2f"%util))
                minute_dict = {}
            time_minute_flag = time
        return minute_dict, dict_all, distance_num, time_minute_flag
    except Exception as e:
        print(e)
        return None, None, None, None