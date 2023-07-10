# Tasktop

针对于负载问题的分析工具，相比loadtask对于细分场景进行了覆盖。

## Usgae

```bash
Usage: tasktop [OPTION...]
A light top, display the process/thread cpu utilization in peroid.

USAGE: tasktop [--help] [-t] [-p TID] [-d DELAY] [-i ITERATION] [-s SORT] [-f
LOGFILE] [-l LIMIT] [-H] [-e D-LIMIT]

EXAMPLES:
    tasktop            # run forever, display the cpu utilization.
    tasktop -t         # display all thread.
    tasktop -p 1100    # only display task with pid 1100.
    tasktop -d 5       # modify the sample interval.
    tasktop -i 3       # output 3 times then exit.
    tasktop -s user    # top tasks sorted by user time.
    tasktop -l 20      # limit the records number no more than 20.
    tasktop -e 10      # limit the d-stack no more than 10, default is 20.
    tasktop -H         # output time string, not timestamp.    
    tasktop -f a.log   # log to a.log.
    tasktop -e 10      # most record 10 d-task stack.

  -d, --delay=DELAY          Sample peroid, default is 3 seconds
  -e, --d-limit=D-LIMIT      Specify the D-LIMIT D tasks's stack to display
  -f, --logfile=LOGFILE      Logfile for result, default
                             /var/log/sysak/tasktop/tasktop.log
  -H, --human                Output human-readable time info.
  -i, --iter=ITERATION       Output times, default run forever
  -l, --r-limit=LIMIT        Specify the top R-LIMIT tasks to display
  -p, --pid=TID              Specify thread TID
  -s, --sort=SORT            Sort the result, available options are user, sys
                             and cpu, default is cpu
  -t, --thread               Thread mode, default process
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
```

## 使用说明

详细的使用案例，请查看`tasktopSelftest`目录下的`test.md`。
