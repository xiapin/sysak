# -*- coding: utf-8 -*-
#!/root/anaconda3/envs/python310
import json
import requests
import datetime
import time
from yaml_instance import sar_config
tz = int(time.strftime('%z')) / 100

config = sar_config()
host_config = config["host_config"]

def get_sql_resp(minutes, table, date):
    try:
        url = host_config +"/api/query"
        if minutes:
            d = {"mode": "last", "time": "%sm"%minutes, "table": table}
            res_last = requests.post(url, json=d)
            ret_last = res_last.content.decode()
            ret_last = json.loads(ret_last)
            return ret_last
        ret_list = []
        now = datetime.datetime.now()
        if date:
            date_distance = datetime.timedelta(hours=date)
        else:
            date_distance = datetime.timedelta(days=1)
        start_date = now - date_distance    #开始时间
        while start_date < now:
            start_date_strf = start_date.strftime("%Y-%m-%d %H:%M:%S")
            end_time = start_date + datetime.timedelta(minutes=30)
            end_time = end_time.strftime("%Y-%m-%d %H:%M:%S")
            # eq: d = {'mode': 'date', 'start': '2023-08-30 06:27:48', 'stop': '2023-08-30 06:57:48', 'tz': -8, 'table': ['cpu_total']}
            d = {"mode": "date", "start": start_date_strf, "stop": end_time, "tz": -tz, "table": table}
            retrys=0
            max_retry=3
            res = requests.post(url, json=d)
            if res.status_code!=requests.codes.ok:
                while retrys<max_retry:
                    res = requests.post(url, json=d)
                    retrys+=1
                    time.sleep(0.5)
                    if res.status_code==requests.codes.ok:
                        break
            ret = res.content.decode()
            start_date=start_date+datetime.timedelta(minutes=30)
            if ret == '{}':
                continue
            ret = json.loads(ret)
            ret_list= ret_list+ ret
        return ret_list
    except Exception as e:
        print(e)
        return []