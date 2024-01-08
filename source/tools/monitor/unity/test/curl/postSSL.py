import json
import requests
import time
from  threading import Thread

class CsslThread(Thread):
    def __init__(self):
        super(CsslThread, self).__init__()
        self.start()

    def run(self):
        url = "http://127.0.0.1:8400/api/ssl"
        data = {"host": "cn.bing.com", "uri": "/"}
        while True:
            try:
                res = requests.post(url, data=json.dumps(data))
            except requests.exceptions.ConnectionError:
                continue
            assert(res.status_code == 200)
            time.sleep(0.1)

ps = []
for i in range(10):
    ps.append(CsslThread())
for p in ps:
    p.join()
