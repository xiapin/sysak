import time
import datetime
import requests
import random
import json

def post_test(d):
    url = "http://127.0.0.1:8400/api/sql"
    res = requests.post(url, json=d)
    ret = res.content.decode()
    print(ret)

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
    post_test(sqlclause)

if __name__ == "__main__":

    q_by_sql()