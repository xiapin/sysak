# -*- coding: utf-8 -*-

import statistics
import datetime
from db import get_sql_resp
from hum_byte_convert import hum_byte_convert
from yaml_instance import load_resp_second_dist


config = load_resp_second_dist()
second_dist = config["config"]["freq"]


def unity_total(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        rets = get_sql_resp(minutes=minutes, table=["cpu_total","meminfo","net_tcp_count","networks","disks","proc_loadavg"], date=date)
        if not rets:
            return
        disks_rets = get_sql_resp(minutes=1, table=["disks"], date=date)
        disks_name_list = []
        for disks_ret in disks_rets:
            if disks_ret.get("labels").get("disk_name") not in disks_name_list:
                disks_name_list.append(disks_ret.get("labels").get("disk_name"))
        print_title = "Time            --cpu--     ---mem--    ---tcp--  -----traffic------"
        title_unity = "Time            cpu_util     mem_util    retran      bytin  bytout  "
        print_unity = "Time             util        util        retran      bytin  bytout  "
        for disk_name in disks_name_list:
            print_title = print_title + ("    --%s--    "% disk_name).ljust(20)
            print_unity = print_unity + ("      util     ").ljust(20)
            title_unity = title_unity + ("      %s"%disk_name + "util    ").ljust(20)
        print_title = print_title + "  --load---"
        print_unity = print_unity + "   load1   "
        title_unity = title_unity + "   load1   "
        print(print_title)
        print(print_unity)
        distance_num = 0
        time_minute_flag = None
        minute_dict = {}
        dict_all= {}
        title_list = title_unity.split(' ')
        title_index_list = []
        title_old_index = 0
        for title in title_list:
            if title:
                y = title_unity.index(title)
                if title_index_list:
                    title_index_list.append(y-title_old_index)
                else:
                    title_index_list.append(y)
                title_old_index = y
        title_list = [val for val in title_list if val]
        title_print_distance_str='{:<' + '}{:<'.join([str(i) for i in title_index_list[1:]]) + '}{:<'+(str(title_index_list[-1])) + '}'
        for ret in rets:
            time = ret.get("time")
            time = datetime.datetime.fromtimestamp(int(time)/1000000).strftime("%m/%d/%y-%H:%M")
            time_strp = datetime.datetime.strptime(time, "%m/%d/%y-%H:%M")
            title = ret.get("title")
            if time == time_minute_flag or time_minute_flag == None:
                if title not in minute_dict.keys():
                    minute_dict[title] = {}
                if title == "networks":
                    if ret.get("values").get("if_ibytes") != None: 
                        if not minute_dict.get(title).get("bytin"):
                            minute_dict[title]["bytin"] = [ret.get("values").get("if_ibytes")]
                        else:
                            minute_dict.get(title).get("bytin").append(ret.get("values").get("if_ibytes"))
                    if ret.get("values").get("if_obytes") != None: 
                        if not minute_dict.get(title).get("bytout"):    
                            minute_dict[title]["bytout"] = [ret.get("values").get("if_obytes")]
                        else:
                            minute_dict.get(title).get("bytout").append(ret.get("values").get("if_obytes"))
                if (time_strp.minute+time_strp.hour*60)%distance_max == 0:
                    minute_dict = append_minute_dict(title, minute_dict, ret)
                    time_minute_flag = time
            else:
                if (time_strp.minute+time_strp.hour*60)%distance_max == 0:
                    
                    # for db_type in minute_dict.keys():
                    # if  db_type == "meminfo":
                    free = (sum(minute_dict.get("meminfo").get("free"))/len(minute_dict.get("meminfo").get("free")))
                    total = (sum(minute_dict.get("meminfo").get("total"))/len(minute_dict.get("meminfo").get("total")))
                    buff = (sum(minute_dict.get("meminfo").get("buff"))/len(minute_dict.get("meminfo").get("buff")))
                    cache = (sum(minute_dict.get("meminfo").get("cach"))/len(minute_dict.get("meminfo").get("cach")))
                    mem_util = (total - free - buff - cache) / total * 100
                    if "meminfo" not in dict_all.keys():
                        dict_all["meminfo"] = [mem_util]
                    else:
                        dict_all.get("meminfo").append(mem_util)
                    # if db_type == "cpu_total":
                    iowait = (sum(minute_dict.get("cpu_total").get("iowait"))/len(minute_dict.get("cpu_total").get("iowait")))
                    steal = (sum(minute_dict.get("cpu_total").get("steal"))/len(minute_dict.get("cpu_total").get("steal")))
                    idle = (sum(minute_dict.get("cpu_total").get("idle"))/len(minute_dict.get("cpu_total").get("idle")))
                    cpu_util = 100-idle-steal-iowait
                    if "cpu_total" not in dict_all.keys():
                        dict_all["cpu_total"] = [cpu_util]
                    else:
                        dict_all.get("cpu_total").append(cpu_util)
                    # if db_type == "net_tcp_count":
                    tcp_retran = (sum(minute_dict.get("net_tcp_count").get("retranssegs"))/sum(minute_dict.get("net_tcp_count").get("outseg")))
                    if "net_tcp_count" not in dict_all.keys():
                        dict_all["net_tcp_count"] = [tcp_retran]
                    else:
                        dict_all.get("net_tcp_count").append(tcp_retran)
                    bytin = (sum(minute_dict.get("networks").get("bytin")))*second_dist
                    bytout = (sum(minute_dict.get("networks").get("bytout")))*second_dist
                    if "networks" not in dict_all.keys():
                        dict_all["networks"]={}
                        dict_all["networks"]["bytin"] = [bytin]
                        dict_all["networks"]["bytout"] = [bytout]
                    else:
                        dict_all.get("networks").get("bytin").append(bytin)
                        dict_all.get("networks").get("bytout").append(bytout)
                    # if db_type == "proc_loadavg":
                    load1 = (sum(minute_dict.get("proc_loadavg").get("load1"))/len(minute_dict.get("proc_loadavg").get("load1")))
                    if "proc_loadavg" not in dict_all.keys():
                        dict_all["proc_loadavg"] = [load1]
                    else:
                        dict_all.get("proc_loadavg").append(load1)
                    # if db_type == "disks":
                    for disk in minute_dict.get("disks").keys():
                        util = sum(minute_dict.get("disks").get(disk))/len(minute_dict.get("disks").get(disk))
                        if "disks" not in dict_all.keys():
                                dict_all["disks"]={}
                                dict_all["disks"][disk] = [util]
                        elif disk not in dict_all.get("disks").keys():
                            dict_all["disks"][disk]=[util]
                        else:
                            dict_all.get("disks").get(disk).append(util)
                    print(("%s"%title_print_distance_str).format(time_minute_flag, "%.2f"%cpu_util , "%.2f"%mem_util, "%.2f"%tcp_retran, hum_byte_convert(bytin), hum_byte_convert(bytout),
                                                                    *("%.2f"%(sum(minute_dict.get("disks").get(i))/len(minute_dict.get("disks").get(i))) for i in disks_name_list),"%.2f"%load1
                                                                    ))
                    distance_num+=1
                    if distance_num >=19:
                        print(print_title)
                        print(print_unity)
                        distance_num = 0
                    minute_dict = {}
                    minute_dict = append_minute_dict(title, minute_dict, ret)
                    time_minute_flag = time
                elif title == "networks":
                    if ret.get("values").get("if_ibytes") != None: 
                        if not minute_dict.get(title).get("bytin"):
                            minute_dict[title]["bytin"] = [ret.get("values").get("if_ibytes")]
                        else:
                            minute_dict.get(title).get("bytin").append(ret.get("values").get("if_ibytes"))
                    if ret.get("values").get("if_obytes") != None: 
                        if not minute_dict.get(title).get("bytout"):    
                            minute_dict[title]["bytout"] = [ret.get("values").get("if_obytes")]
                        else:
                            minute_dict.get(title).get("bytout").append(ret.get("values").get("if_obytes"))
                else:
                    continue

        if minute_dict:
            # for db_type in minute_dict.keys():
            # if  db_type == "meminfo":
            free = (sum(minute_dict.get("meminfo").get("free"))/len(minute_dict.get("meminfo").get("free")))
            total = (sum(minute_dict.get("meminfo").get("total"))/len(minute_dict.get("meminfo").get("total")))
            buff = (sum(minute_dict.get("meminfo").get("buff"))/len(minute_dict.get("meminfo").get("buff")))
            cache = (sum(minute_dict.get("meminfo").get("cach"))/len(minute_dict.get("meminfo").get("cach")))
            mem_util = (total - free - buff - cache) / total * 100
            if "meminfo" not in dict_all.keys():
                dict_all["meminfo"] = [mem_util]
            else:
                dict_all.get("meminfo").append(mem_util)
            # if db_type == "cpu_total":
            iowait = (sum(minute_dict.get("cpu_total").get("iowait"))/len(minute_dict.get("cpu_total").get("iowait")))
            steal = (sum(minute_dict.get("cpu_total").get("steal"))/len(minute_dict.get("cpu_total").get("steal")))
            idle = (sum(minute_dict.get("cpu_total").get("idle"))/len(minute_dict.get("cpu_total").get("idle")))
            cpu_util = 100-idle-steal-iowait
            if "cpu_total" not in dict_all.keys():
                dict_all["cpu_total"] = [cpu_util]
            else:
                dict_all.get("cpu_total").append(cpu_util)
            # if db_type == "net_tcp_count":
            tcp_retran = (sum(minute_dict.get("net_tcp_count").get("retranssegs"))/sum(minute_dict.get("net_tcp_count").get("outseg")))
            if "net_tcp_count" not in dict_all.keys():
                dict_all["net_tcp_count"] = [tcp_retran]
            else:
                dict_all.get("net_tcp_count").append(tcp_retran)
            bytin = (sum(minute_dict.get("networks").get("bytin")))*second_dist
            bytout = (sum(minute_dict.get("networks").get("bytout")))*second_dist
            if "networks" not in dict_all.keys():
                dict_all["networks"]={}
                dict_all["networks"]["bytin"] = [bytin]
                dict_all["networks"]["bytout"] = [bytout]
            else:
                dict_all.get("networks").get("bytin").append(bytin)
                dict_all.get("networks").get("bytout").append(bytout)
            # if db_type == "proc_loadavg":
            load1 = (sum(minute_dict.get("proc_loadavg").get("load1"))/len(minute_dict.get("proc_loadavg").get("load1")))
            if "proc_loadavg" not in dict_all.keys():
                dict_all["proc_loadavg"] = [load1]
            else:
                dict_all.get("proc_loadavg").append(load1)
            # if db_type == "disks":
            for disk in minute_dict.get("disks").keys():
                util = sum(minute_dict.get("disks").get(disk))/len(minute_dict.get("disks").get(disk))
                if "disks" not in dict_all.keys():
                        dict_all["disks"]={}
                        dict_all["disks"][disk] = [util]
                elif disk not in dict_all.get("disks").keys():
                    dict_all["disks"][disk]=[util]
                else:
                    dict_all.get("disks").get(disk).append(util)
                # print(("%s"%title_print_distance_str).format(time_minute_flag, "%.2f"%cpu_util , "%.2f"%mem_util, "%.2f"%tcp_retran, hum_byte_convert(bytin), hum_byte_convert(bytout), disk, "%.2f"%util, "%.2f"%load1))
            print(("%s"%title_print_distance_str).format(time_minute_flag, "%.2f"%cpu_util , "%.2f"%mem_util, "%.2f"%tcp_retran, hum_byte_convert(bytin), hum_byte_convert(bytout),
                                                                    *("%.2f"%(sum(minute_dict.get("disks").get(i))/len(minute_dict.get("disks").get(i))) for i in disks_name_list),"%.2f"%load1
                                                                    ))
            distance_num+=1
            if distance_num >=19:
                # print("Time            --cpu--     ---mem--    ---tcp-- -----traffic----   --name--    --vdb---   --load---")
                # print("Time             util         util       retran    bytin  bytout      name       util        load1  ")
                print(print_title)
                print(print_unity)
                distance_num =0
        print(("\n%s"%title_print_distance_str).format("MAX" , "%.2f"%max(dict_all.get("cpu_total")),
                                                                "%.2f"%max(dict_all.get("meminfo")),
                                                                "%.2f"%max(dict_all.get("net_tcp_count")),
                                                                hum_byte_convert(max(dict_all.get("networks").get("bytin"))),
                                                                hum_byte_convert(max(dict_all.get("networks").get("bytout"))),
                                                                *("%.2f"%max(dict_all.get("disks").get(i)) for i in disks_name_list),
                                                                "%.2f"%max(dict_all.get("proc_loadavg"))
                                                            ))
        print(("%s"%title_print_distance_str).format("MEAN" , "%.2f"%statistics.mean(dict_all.get("cpu_total")),
                                                                "%.2f"%statistics.mean(dict_all.get("meminfo")),
                                                                "%.2f"%statistics.mean(dict_all.get("net_tcp_count")),
                                                                hum_byte_convert(statistics.mean(dict_all.get("networks").get("bytin"))),
                                                                hum_byte_convert(statistics.mean(dict_all.get("networks").get("bytout"))),
                                                                *("%.2f"%statistics.mean(dict_all.get("disks").get(i)) for i in disks_name_list),
                                                                "%.2f"%statistics.mean(dict_all.get("proc_loadavg"))
                                                            ))
        print(("%s"%title_print_distance_str).format("MIN", "%.2f"%min(dict_all.get("cpu_total")),
                                                                "%.2f"%min(dict_all.get("meminfo")),
                                                                "%.2f"%min(dict_all.get("net_tcp_count")),
                                                                hum_byte_convert(min(dict_all.get("networks").get("bytin"))),
                                                                hum_byte_convert(min(dict_all.get("networks").get("bytout"))),
                                                                *("%.2f"%min(dict_all.get("disks").get(i)) for i in disks_name_list),
                                                                "%.2f"%min(dict_all.get("proc_loadavg"))
                                                            ))
    except Exception as e:
        print(e)
        return


def append_minute_dict(title,minute_dict, ret):
    try:
        if title not in minute_dict.keys():
            minute_dict[title] = {}
        if title=="disks":  #IO
            disk_name = ret.get("labels").get("disk_name") 
            
            if disk_name not in minute_dict.get(title).keys():
                minute_dict[title][disk_name] = {}
            if ret.get("values").get("busy") != None: 
                if not minute_dict.get(title).get(disk_name):
                    minute_dict[title][disk_name] = [ret.get("values").get("busy")]
                else:
                    minute_dict.get(title).get(disk_name).append(ret.get("values").get("busy"))
        if title == "meminfo":  #mem
            if ret.get("values").get("MemTotal") != None: 
                if not minute_dict.get(title).get("total"):
                    minute_dict[title]["total"] = [ret.get("values").get("MemTotal")]
                else:
                    minute_dict.get(title).get("total").append(ret.get("values").get("MemTotal"))
            if ret.get("values").get("MemFree") != None: 
                if not minute_dict.get(title).get("free"):    
                    minute_dict[title]["free"] = [ret.get("values").get("MemFree")]
                else:
                    minute_dict.get(title).get("free").append(ret.get("values").get("MemFree"))
            if ret.get("values").get("user_buffers") != None: 
                if not minute_dict.get(title).get("buff"):    
                    minute_dict[title]["buff"] = [ret.get("values").get("user_buffers")]
                else:
                    minute_dict.get(title).get("buff").append(ret.get("values").get("user_buffers"))
            if ret.get("values").get("Cached") != None: 
                if not minute_dict.get(title).get("cach"):    
                    minute_dict[title]["cach"] = [ret.get("values").get("Cached")]
                else:
                    minute_dict.get(title).get("cach").append(ret.get("values").get("Cached"))
        if title == "cpu_total":  #cpu
            if ret.get("values").get("iowait") != None: 
                if not minute_dict.get(title).get("iowait"):
                    minute_dict[title]["iowait"] = [ret.get("values").get("iowait")]
                else:
                    minute_dict.get(title).get("iowait").append(ret.get("values").get("iowait"))
            if ret.get("values").get("steal") != None: 
                if not minute_dict.get(title).get("steal"):    
                    minute_dict[title]["steal"] = [ret.get("values").get("steal")]
                else:
                    minute_dict.get(title).get("steal").append(ret.get("values").get("steal"))
            if ret.get("values").get("idle") != None: 
                if not minute_dict.get(title).get("idle"):    
                    minute_dict[title]["idle"] = [ret.get("values").get("idle")]
                else:
                    minute_dict.get(title).get("idle").append(ret.get("values").get("idle"))
        if title == "net_tcp_count":  #tcp
            if ret.get("values").get("OutSegs") != None: 
                if not minute_dict.get(title).get("outseg"):
                    minute_dict[title]["outseg"] = [ret.get("values").get("OutSegs")]
                else:
                    minute_dict.get(title).get("outseg").append(ret.get("values").get("OutSegs"))
            if ret.get("values").get("RetransSegs") != None: 
                if not minute_dict.get(title).get("retranssegs"):    
                    minute_dict[title]["retranssegs"] = [ret.get("values").get("RetransSegs")]
                else:
                    minute_dict.get(title).get("retranssegs").append(ret.get("values").get("RetransSegs"))
        if title == "proc_loadavg":  #load
            if ret.get("values").get("load1") != None: 
                if not minute_dict.get(title).get("load1"):
                    minute_dict[title]["load1"] = [ret.get("values").get("load1")]
                else:
                    minute_dict.get(title).get("load1").append(ret.get("values").get("load1"))
        return minute_dict
    except Exception as e:
        print(e)
        return minute_dict

        