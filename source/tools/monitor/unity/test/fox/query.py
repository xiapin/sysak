import time
import datetime
import requests
import random
import json


def post_test(d):
    url = "http://127.0.0.1:8400/api/query"
    res = requests.post(url, json=d)
    ret = json.loads(res.content.decode())
    print(len(ret))


def q_table():
    post_test({"mode": "table", "duration": "1"})


def q_by_table():
    post_test({"mode": "last", "time": "5m", "table": ["per_sirqs"]})


def q_by_date():
    now = datetime.datetime.now()
    delta1 = datetime.timedelta(minutes=0)
    delta2 = datetime.timedelta(minutes=2)
    d1 = now - delta1
    d2 = d1 - delta2
    s1 = d1.strftime("%Y-%m-%d %H:%M:%S")
    s2 = d2.strftime("%Y-%m-%d %H:%M:%S")

    print(s2, s1)
    post_test({"mode": "date", "start": s2, "stop": s1, "tz": 8, "table": ["cpu_total"]})

def q_by_sql():
    post_test("SELECT net_rx, rcu FROM per_sirqs WHERE time > NOW(-10) and cpu = cpu1")
    now = datetime.datetime.now()
    delta1 = datetime.timedelta(hours=8)
    delta2 = datetime.timedelta(minutes=5)
    d1 = now + delta1
    d2 = d1 - delta2
    s1 = d1.strftime("%Y-%m-%d %H:%M:%S")
    s2 = d2.strftime("%Y-%m-%d %H:%M:%S")
    sqlclause = "SELECT net_rx, rcu FROM per_sirqs WHERE time BETWEEN '" + s2 + "' and '" + s1 + "' and cpu = cpu1"
    post_test({"mode": "sql", "sql": sqlclause})


if __name__ == "__main__":
    # post_test({"mode": "last", "time": "4m"})
    # q_table()
    # q_by_table()
    q_by_date()
