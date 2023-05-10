import time
import requests
import random
import json


def randomNum():
    return random.randint(0, 1000) / 100.0


def post_test(session):
    url = "http://127.0.0.1:8400/api/"
    mod = random.choice(("sum", "sub"))
    url += mod
    d = {"num1": randomNum(), "num2": randomNum()}
    res = session.post(url, json=d)
    ret = res.content.decode()
    if mod == "sum":
        vLocal = d["num1"] + d["num2"]
        vRet = json.loads(ret)[mod]
    else:
        vLocal = d["num1"] - d["num2"]
        vRet = json.loads(ret)[mod]
    assert (abs(vRet - vLocal) <= 1e-9)


if __name__ == "__main__":
    session = requests.session()
    while True:
        post_test(session)
        time.sleep(random.randint(1, 100)/100.0)