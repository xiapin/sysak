# -*- coding: utf-8 -*-
import statistics
import datetime
from db import get_sql_resp
from utils import get_print_title_distance


def io_data_show(distance_max=5, minutes=50, date=1):
    try:
        if not distance_max:
            distance_max = 5
        rets = get_sql_resp(minutes=minutes, table=["disks"],date=date)
        distance_num = 0
        time_minute_flag = None
        minute_cpu_dict = {}
        cpu_dict_all= {}
        print("Time           -------------------------------------------------------------IO---------------------------------------------------------------")
        print("Time             disk_name    inflight     backlog    rmsec    util    wkb    xfers    bsize    wmsec   rkb      writes   wmerge  rmerge  reads")
        title_io = "Time             disk_name    inflight     backlog    rmsec    busy    wkb    xfers    bsize    wmsec   rkb      writes   wmerge  rmerge  reads"
        title_print_distance_str = get_print_title_distance(title_io)
        endtime = datetime.datetime.fromtimestamp(int(rets[-1].get("time"))/1000000).strftime("%m/%d/%y-%H:%M")
        endtime_strp = datetime.datetime.strptime(endtime, "%m/%d/%y-%H:%M")
        title_list = title_io.split(' ')
        title_list = [val for val in title_list if val]
        cpu_dict_all = {}
        for i in rets:
            time = i.get("time")
            time = datetime.datetime.fromtimestamp(int(time)/1000000).strftime("%m/%d/%y-%H:%M")
            time_strp = datetime.datetime.strptime(time, "%m/%d/%y-%H:%M")
            if (time_strp.minute+time_strp.hour*60)%distance_max != 0:
                continue
            if time == time_minute_flag or time_minute_flag == None:
                disk_name = i.get("labels").get("disk_name")
                for k in title_list[2:]:
                    if i.get("labels").get("disk_name") not in minute_cpu_dict.keys():
                        minute_cpu_dict[disk_name] = {}
                    elif k not in minute_cpu_dict.get(disk_name).keys():
                        if i.get("values").get(k) != None: 
                            minute_cpu_dict[disk_name][k] = [i.get("values").get(k)]
                    else:
                        minute_cpu_dict.get(disk_name).get(k).append(i.get("values").get(k))
                    if i.get("labels").get("disk_name") not in cpu_dict_all.keys():
                        cpu_dict_all[disk_name] = {}
                    if k not in cpu_dict_all.get(disk_name).keys():
                        cpu_dict_all[disk_name][k] = []
                time_minute_flag = time
            else:
                distance_num+=1
                if distance_num >=5:
                    print("Time           -------------------------------------------------------------IO---------------------------------------------------------------")
                    print("Time             disk_name    inflight     backlog    rmsec    util    wkb    xfers    bsize    wmsec   rkb      writes   wmerge  rmerge  reads")
                    distance_num = 0
                if time_strp+datetime.timedelta(minutes=distance_max) > endtime_strp: 
                    for k in minute_cpu_dict.keys():
                        inflight = (sum(minute_cpu_dict.get(k).get("inflight"))/len(minute_cpu_dict.get(k).get("inflight")))
                        backlog = (sum(minute_cpu_dict.get(k).get("backlog"))/len(minute_cpu_dict.get(k).get("backlog")))
                        rmsec = (sum(minute_cpu_dict.get(k).get("rmsec"))/len(minute_cpu_dict.get(k).get("rmsec")))
                        busy = (sum(minute_cpu_dict.get(k).get("busy"))/len(minute_cpu_dict.get(k).get("busy")))
                        wkb = (sum(minute_cpu_dict.get(k).get("wkb"))/len(minute_cpu_dict.get(k).get("wkb")))
                        xfers = (sum(minute_cpu_dict.get(k).get("xfers"))/len(minute_cpu_dict.get(k).get("xfers")))
                        bsize = (sum(minute_cpu_dict.get(k).get("bsize"))/len(minute_cpu_dict.get(k).get("bsize")))
                        wmsec = (sum(minute_cpu_dict.get(k).get("wmsec"))/len(minute_cpu_dict.get(k).get("wmsec")))
                        rkb = (sum(minute_cpu_dict.get(k).get("rkb"))/len(minute_cpu_dict.get(k).get("rkb")))
                        writes = (sum(minute_cpu_dict.get(k).get("writes"))/len(minute_cpu_dict.get(k).get("writes")))
                        wmerge = (sum(minute_cpu_dict.get(k).get("wmerge"))/len(minute_cpu_dict.get(k).get("wmerge")))
                        rmerge = (sum(minute_cpu_dict.get(k).get("rmerge"))/len(minute_cpu_dict.get(k).get("rmerge")))
                        reads = (sum(minute_cpu_dict.get(k).get("reads"))/len(minute_cpu_dict.get(k).get("reads")))
                        cpu_dict_all.get(k).get("inflight").append(inflight)
                        cpu_dict_all.get(k).get("backlog").append(backlog)
                        cpu_dict_all.get(k).get("rmsec").append(rmsec)
                        cpu_dict_all.get(k).get("busy").append(busy)
                        cpu_dict_all.get(k).get("wkb").append(wkb)
                        cpu_dict_all.get(k).get("xfers").append(xfers)                
                        cpu_dict_all.get(k).get("bsize").append(bsize)                
                        cpu_dict_all.get(k).get("wmsec").append(wmsec)                
                        cpu_dict_all.get(k).get("rkb").append(rkb)                
                        cpu_dict_all.get(k).get("writes").append(writes)                
                        cpu_dict_all.get(k).get("wmerge").append(wmerge)                
                        cpu_dict_all.get(k).get("rmerge").append(rmerge)                
                        cpu_dict_all.get(k).get("reads").append(reads)
                        print(("%s"%title_print_distance_str).format(time_minute_flag, k, "%.2f"%inflight,"%.2f"%backlog, "%.2f"%rmsec,"%.2f"%busy, "%.2f"%wkb,"%.2f"%xfers,
                                                                        "%.2f"%bsize,"%.2f"%wmsec,"%.2f"%rkb,"%.2f"%writes,"%.2f"%wmerge,"%.2f"%rmerge,"%.2f"%reads
                                                                        ))
                    for k in cpu_dict_all.keys():
                        inflight = (sum(minute_cpu_dict.get(k).get("inflight"))/len(minute_cpu_dict.get(k).get("inflight")))
                        backlog = (sum(minute_cpu_dict.get(k).get("backlog"))/len(minute_cpu_dict.get(k).get("backlog")))
                        rmsec = (sum(minute_cpu_dict.get(k).get("rmsec"))/len(minute_cpu_dict.get(k).get("rmsec")))
                        busy = (sum(minute_cpu_dict.get(k).get("busy"))/len(minute_cpu_dict.get(k).get("busy")))
                        wkb = (sum(minute_cpu_dict.get(k).get("wkb"))/len(minute_cpu_dict.get(k).get("wkb")))
                        xfers = (sum(minute_cpu_dict.get(k).get("xfers"))/len(minute_cpu_dict.get(k).get("xfers")))
                        bsize = (sum(minute_cpu_dict.get(k).get("bsize"))/len(minute_cpu_dict.get(k).get("bsize")))
                        wmsec = (sum(minute_cpu_dict.get(k).get("wmsec"))/len(minute_cpu_dict.get(k).get("wmsec")))
                        rkb = (sum(minute_cpu_dict.get(k).get("rkb"))/len(minute_cpu_dict.get(k).get("rkb")))
                        writes = (sum(minute_cpu_dict.get(k).get("writes"))/len(minute_cpu_dict.get(k).get("writes")))
                        wmerge = (sum(minute_cpu_dict.get(k).get("wmerge"))/len(minute_cpu_dict.get(k).get("wmerge")))
                        rmerge = (sum(minute_cpu_dict.get(k).get("rmerge"))/len(minute_cpu_dict.get(k).get("rmerge")))
                        reads = (sum(minute_cpu_dict.get(k).get("reads"))/len(minute_cpu_dict.get(k).get("reads")))
                        print(("%s"%title_print_distance_str).format(time, k ,"%.2f"%inflight,"%.2f"%backlog, "%.2f"%rmsec,"%.2f"%busy, "%.2f"%wkb,"%.2f"%xfers,
                                                                        "%.2f"%bsize,"%.2f"%wmsec,"%.2f"%rkb,"%.2f"%writes,"%.2f"%wmerge,"%.2f"%rmerge,"%.2f"%reads
                                                                        ))
                    for k in cpu_dict_all.keys():
                        print(("\n%s"%title_print_distance_str).format("MAX", k, "%.2f"%max(cpu_dict_all.get(k).get("inflight")),
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("backlog")),
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("rmsec")),
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("busy")),
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("wkb")),
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("xfers")),
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("bsize")),
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("wmsec")) ,           
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("rkb")),       
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("writes")),           
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("wmerge")),               
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("rmerge")),          
                                                                            "%.2f"%max(cpu_dict_all.get(k).get("reads"))))
                        print(("%s"%title_print_distance_str).format("MEAN", k, "%.2f"%statistics.mean(cpu_dict_all.get(k).get("inflight")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("backlog")), 
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("rmsec")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("busy")), 
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("wkb")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("xfers")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("bsize")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("wmsec")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("rkb")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("writes")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("wmerge")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("rmerge")),
                                                                            "%.2f"%statistics.mean(cpu_dict_all.get(k).get("reads"))))

                        print(("%s"%title_print_distance_str).format("MIN", k, "%.2f"%min(cpu_dict_all.get(k).get("inflight")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("backlog")), 
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("rmsec")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("busy")), 
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("wkb")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("xfers")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("bsize")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("wmsec")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("rkb")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("writes")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("wmerge")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("rmerge")),
                                                                            "%.2f"%min(cpu_dict_all.get(k).get("reads"))))
                    break
                else:
                    for k in minute_cpu_dict.keys():
                        inflight = (sum(minute_cpu_dict.get(k).get("inflight"))/len(minute_cpu_dict.get(k).get("inflight")))
                        backlog = (sum(minute_cpu_dict.get(k).get("backlog"))/len(minute_cpu_dict.get(k).get("backlog")))
                        rmsec = (sum(minute_cpu_dict.get(k).get("rmsec"))/len(minute_cpu_dict.get(k).get("rmsec")))
                        busy = (sum(minute_cpu_dict.get(k).get("busy"))/len(minute_cpu_dict.get(k).get("busy")))
                        wkb = (sum(minute_cpu_dict.get(k).get("wkb"))/len(minute_cpu_dict.get(k).get("wkb")))
                        xfers = (sum(minute_cpu_dict.get(k).get("xfers"))/len(minute_cpu_dict.get(k).get("xfers")))
                        bsize = (sum(minute_cpu_dict.get(k).get("bsize"))/len(minute_cpu_dict.get(k).get("bsize")))
                        wmsec = (sum(minute_cpu_dict.get(k).get("wmsec"))/len(minute_cpu_dict.get(k).get("wmsec")))
                        rkb = (sum(minute_cpu_dict.get(k).get("rkb"))/len(minute_cpu_dict.get(k).get("rkb")))
                        writes = (sum(minute_cpu_dict.get(k).get("writes"))/len(minute_cpu_dict.get(k).get("writes")))
                        wmerge = (sum(minute_cpu_dict.get(k).get("wmerge"))/len(minute_cpu_dict.get(k).get("wmerge")))
                        rmerge = (sum(minute_cpu_dict.get(k).get("rmerge"))/len(minute_cpu_dict.get(k).get("rmerge")))
                        reads = (sum(minute_cpu_dict.get(k).get("reads"))/len(minute_cpu_dict.get(k).get("reads")))
                        cpu_dict_all.get(k).get("inflight").append(inflight)
                        cpu_dict_all.get(k).get("backlog").append(backlog)
                        cpu_dict_all.get(k).get("rmsec").append(rmsec)
                        cpu_dict_all.get(k).get("busy").append(busy)
                        cpu_dict_all.get(k).get("wkb").append(wkb)
                        cpu_dict_all.get(k).get("xfers").append(xfers)                
                        cpu_dict_all.get(k).get("bsize").append(bsize)                
                        cpu_dict_all.get(k).get("wmsec").append(wmsec)                
                        cpu_dict_all.get(k).get("rkb").append(rkb)                
                        cpu_dict_all.get(k).get("writes").append(writes)                
                        cpu_dict_all.get(k).get("wmerge").append(wmerge)                
                        cpu_dict_all.get(k).get("rmerge").append(rmerge)                
                        cpu_dict_all.get(k).get("reads").append(reads)
                        print(("%s"%title_print_distance_str).format(time_minute_flag, k, "%.2f"%inflight,"%.2f"%backlog, "%.2f"%rmsec,"%.2f"%busy, "%.2f"%wkb,"%.2f"%xfers,
                                                                        "%.2f"%bsize,"%.2f"%wmsec,"%.2f"%rkb,"%.2f"%writes,"%.2f"%wmerge,"%.2f"%rmerge,"%.2f"%reads
                                                                        ))
                        
                        disk_name = i.get("labels").get("disk_name")
                        for title in title_list[2:]:
                            if i.get("values").get(title) != None: 
                                minute_cpu_dict[disk_name][title] = [i.get("values").get(title)]
                    time_minute_flag = time
    except Exception as e:
        print(e)
        return