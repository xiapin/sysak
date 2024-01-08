# 数据查询接口

## 常规查询

[常规查询入口](http://127.0.0.1:8400/api/query)

查询的mode可以为last table date，mode不可缺省。

一次最多查询200条数据。

数据最多保存7天。

### mode == last
mode为last时，需要的参数为table、time。

查找table表从当前时间开始time时长的数据。

time格式为%dh%dm%ds，其中h、m、s分别为时、分、秒的单位，若单位缺省，则默认为秒。

若table缺省，则查询所有表格的数据。

python脚本查询示例：

```python
def q_by_table():
    url = "http://127.0.0.1:8400/api/query"
    d = {"mode": "last", "time": "5m", "table": ["per_sirqs"]}
    res = requests.post(url, json=d)
    ret = res.content.decode()
    print(ret)
```

curl查询示例：

```bash
curl --header "Content-Type: application/json" \
     --request POST \
     --data "{\"mode\":\"last\",\"time\":\"5m\",\"table\":[\"cpus\"]}" \
     http://100.83.167.10:3350/api/query
```

### mode == table

mode为table时，需要的参数为duration。

查询从当前时间开始前duration小时的所有数据。

duration单位为小时，缺省则默认为2小时。不能超过24小时。

python脚本查询示例：

```python
def q_table():
    url = "http://127.0.0.1:8400/api/query"
    d = {"mode": "table", "duration": "1"}
    res = requests.post(url, json=d)
    ret = res.content.decode()
    print(ret)
```

curl查询示例：

```bash
curl --header "Content-Type: application/json" \
     --request POST \
     --data "{\"mode\":\"table\",\"duration\":\"1\"}" \
     http://100.83.167.10:3350/api/query
```

### mode == date

mode为date时，需要的参数为start、stop、tz、table。

查询从start到stop之间，table的数据，其中start和stop为tz时区的时间。

start和stop的格式为%Y-%m-%d %H:%M:%S，start和stop之间不能超过24小时。

tz缺省则为0。

若table缺省，则查询所有表格的数据。

python脚本查询示例：

```python
def q_by_date():
    now = datetime.datetime.now()
    delta1 = datetime.timedelta(days=1, hours=1)
    delta2 = datetime.timedelta(minutes=5)
    d1 = now - delta1
    d2 = d1 - delta2
    s1 = d1.strftime("%Y-%m-%d %H:%M:%S")
    s2 = d2.strftime("%Y-%m-%d %H:%M:%S")

    print(s1, s2)
    url = "http://127.0.0.1:8400/api/query"
    d = {"mode": "date", "start": s2, "stop": s1, "tz": 8, "table": ["cpu_total", "cpus"]}
    res = requests.post(url, json=d)
    ret = res.content.decode()
    print(ret)
```

curl查询示例：

```bash
curl --header "Content-Type: application/json" \
     --request POST \
     --data "{\"mode\":\"date\",\"start\":\"2023-07-18 17:25:00\",\"stop\":\"2023-07-18 17:30:00\",\"tz\":8,\"table\":[\"cpu_total\", \"cpus\"]}" \
     http://100.83.167.10:3350/api/query
```

## sql语句查询
[sql语句查询入口](http://127.0.0.1:8400/api/sql)

### 查询语句要求
根据sql语句进行查询，查询的语句基本遵循sql语法，但有如下规定 ：

1.  where中必须包含对time的限制

    可以使用两种方式对time进行限制
    * between time1 and time2

      time1 和time2的格式为%Y-%m-%d %H:%M:%S
    * time>NOW(-secs)
    
       如：
      ```bash
       SELECT * FROM tbl_a WHERE time BETWEEN time1 and time2
       SELECT * FROM tbl_b WHERE time > NOW(-10)
      ```
      

2.  where中仅可使用“=”对tag进行筛选
    * 正确写法
      ```SELECT * FROM tbl_a WHERE time > NOW(-10) and cpu = cpu1```
    * 错误写法
      ```SELECT * FROM tbl_a WHERE time > NOW(-10) and cpu > cpu1```

3.  仅支持筛选出fields的值
    关于field的定义可以参考[行协议说明](/guide/outline.md)

### 查询示例

```python
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
```