# -*- coding: utf-8 -*-
import os,sys,re
import traceback
from loglist import kmsg_log_list
from datetime import date, datetime, timedelta
import requests
import json

KMSG_FILE_PATH = "/proc/kmsg"
WARNING_REPEAT_INTERVAL = 1 # mins

def push_logwarn(text,pattern):
    try:
        url = "http://127.0.0.1:8400/api/alert"
        data = {"alert_item": "kmsg",
            "alert_category": "MONITOR",
            "alert_source_type": "kmsg",
            "alert_level": "WARNING",
            "status": "FIRING",
            "labels": {"pattern":pattern
            },
            "annotations": {
                "summary": text.replace("\"","").replace("\'","")
            },
        }
        res = requests.post(url, data=json.dumps(data))
        print(res.content, res)
    except:
        traceback.print_exc()

def main():
    fd = open(KMSG_FILE_PATH, 'r')
    try:
        warn_end_time = datetime.now()
        warnlog = {}
        while True:
            warn_ignore = 0
            text_line = fd.readline()
            if text_line:
                for p in kmsg_log_list:
                    match = re.search(p,text_line)
                    if match:
                        warn_end_time = datetime.now()
                        if p in warnlog:
                            if (warn_end_time - warnlog[p]).seconds < WARNING_REPEAT_INTERVAL*60:
                                warn_ignore = 1
                        if warn_ignore == 0:
                            warnlog[p] = warn_end_time
                            push_logwarn(text_line,p)
            else:
                break
    except:
        traceback.print_exc()
        fd.close()

if __name__ == "__main__":
    main()
