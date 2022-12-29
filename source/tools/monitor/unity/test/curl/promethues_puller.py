import time
import requests
import random


def randomNum():
    return random.randint(0, 1000) / 100.0


def pull_test():
    url = random.choice(("http://127.0.0.1:8400/export",
                         "http://127.0.0.1:8400/export/metrics",
                         "http://127.0.0.1:8400/"
                         ))
    res = requests.get(url)
    assert (res.status_code == 200)
    assert (len(res.content) > 100)


if __name__ == "__main__":
    while True:
        pull_test()
        time.sleep(random.randint(1, 100)/100.0)
