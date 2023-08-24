id=$1
echo $pid

EXPIRE=$((2*24*60*60))
# EXPIRE=$((5*60))
LOG_DIR="/var/log/sysak/cpuhigh/"
STOP_FILE="${LOG_DIR}/stop"
TIME_FORMAT_STR=""
i=0

collect_func() {
    perf record -F 99 -a -g -o $1 -- sleep 30
    echo $? >> ${LOG_DIR}/shell.log
    sleep 1
}

# /var/log/sysak/cpuhigh/2023-07-31/09/37_20.log
#   2   3   4      5        6        7     8

get_time_str() {
    time_str=$1
    format_time=""
    arr=`echo $time_str | awk -F '[./_]' '{print $6,$7,$8,$9}'`
    #     echo $arr
    N=1
    for time in $arr
    do
        # echo "[$num]"
        if [ $N -eq 1 ]
        then
            format_time="$time"
        elif [ $N -eq 2 ]
        then
            format_time="$format_time $time"
        else
            format_time="$format_time:$time"
        fi
        
        ((N++))
    done
    #     echo $format_time
    TIME_FORMAT_STR=$format_time
}

clean_expire_files() {
    now=`date "+%s"`
    for date_dir in ${LOG_DIR}*
    do
        if test -d $date_dir
        then
            for hour_dir in ${date_dir}/*
            do
                if test -d $hour_dir
                then
                    for time_file in ${hour_dir}/*
                    do
                        if test -f $time_file
                        then
                            get_time_str "$time_file"
                            ts=`date -d "$TIME_FORMAT_STR" +%s`
                            delta=`expr $now - $ts`
                            if [ $delta -gt $EXPIRE ]
                            then
                                echo "delte file=${time_file}."
                                rm -f $time_file
                            #else
                            #    echo "file is available=${time_file}."
                            fi
                        fi
                    done
                fi
                
            done
        fi
    done
}

main() {
    mkdir -p $LOG_DIR
    yum install perf -y
    i=0
    while ((1))
    do
        if [ -a $STOP_FILE ]; then
            echo "stop profileing"
            exit 0
        fi
        # _day0=$(date "+%d")
        _date=$(date "+%Y-%m-%d")
        _hour=$(date "+%H")
        _time=$(date "+%M_%S")
        if [ ! -d ${LOG_DIR}/${_date} ]; then
            mkdir -p ${LOG_DIR}/${_date}
        fi
        if [ -d ${LOG_DIR}/${_date}/${_hour} ]; then
            collect_func ${LOG_DIR}/${_date}/${_hour}/${_time}.log
        else
            mkdir -p ${LOG_DIR}/${_date}/${_hour}
            collect_func ${LOG_DIR}/${_date}/${_hour}/${_time}.log
        fi
        i=$((i+1))
        #3*60*12, 12hours
        if [ $i -gt 2160 ]; then
                clean_expire_files
                i=0
        fi
    done
}

main

# clean_expire_files
# get_ts "/var/log/sysak/cpuhigh/2023-07-31/09/37_20.log"
