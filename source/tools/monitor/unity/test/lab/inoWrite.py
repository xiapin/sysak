
import os
import time

i = 1
with open("./test.txt", 'w') as f:
    while True:
        time.sleep(i)
        i += 1
        os.write(f.fileno(), "hello.".encode())
        print(i)