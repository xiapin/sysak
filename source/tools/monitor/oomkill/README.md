# 功能说明
用户态oomkill服务

# 使用说明
sysak oomkill 
-m PERCENT[,KILL_PERCENT] set available memory minimum to PERCENT of total
                            (default 10 %).
                            oomkill sends SIGTERM once below PERCENT, then
                            SIGKILL once below KILL_PERCENT (default PERCENT/2).

  -M SIZE[,KILL_SIZE]       set available memory minimum to SIZE KiB

  -k                        kill mode: 0, 1, 2, 3 (default 1)

  -i                        cpu iowait value (default 30)

  -I                        cpu system value (default 30)

  -n                        enable d-bus notifications

  -N /PATH/TO/SCRIPT        call script after oom kill

  -g                        kill all processes within a process group

  -d                        enable debugging messages

  -v                        print version information and exit

  -r INTERVAL               memory report interval in seconds (default 1), set
                            to 0 to disable completely

  -p                        set niceness of oomkill to -20 and oom_score_adj to
                            -100

  --ignore-root-user        do not kill processes owned by root

  --prefer REGEX            prefer to kill processes matching REGEX

  --avoid REGEX             avoid killing processes matching REGEX

  --ignore REGEX            ignore processes matching REGEX

  --dryrun                  dry run (do not kill any processes)

  -h, --help                this help text



