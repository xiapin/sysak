#!/bin/sh
#****************************************************************#
# ScriptName: ulockcheck.sh
# Author: zhao.hang@linux.alibaba.com
# Create Date: 2021-10-18 18:30
# Function:
#***************************************************************#

CALLTRACE_LOG="parase_calltrace_log"
RAW_CALLTRACE_LOG="raw_calltrace_log"
SYSAK_BBOX="/proc/sysak/bbox"
LOG_DIR="/var/log/sysak/ulockcheck/"

usage() {
    echo "usage: ulockcheck <option> [<args>]"
	echo "ulockcheck: thread mutex monitor tool "
	echo "options: -h                       help information"
	echo "         -p <pid>                 enable futex monitor"
	echo "         -a <path>                show lock owner and top 5 futex monitor result"
	echo "                                  and save all monitor result in path，fault path:/var/log/sysak/ulockcheck/bbox_log"
	echo "         -s <pid>                 show specifed thread futex monitor result"
	echo "         -g <path>                parse specify <path> calltrace to log file, fault log file:/var/log/sysak/ulockcheck/calltrace_log"
	echo "         -l <thresold>            set lock delay thresold"
	echo "         -w <thresold>            set wait delay thresold"
	echo "         -t <0/1>                 disable/enable show user calltrace"
	echo "         -d                       disable futex monitor"
}

enable_ulockcheck(){
    if [[ -f /proc/sysak/ulockcheck/enable ]]; then
        echo 1 > /proc/sysak/ulockcheck/enable
    fi

    if [[ -f /proc/sysak/ulockcheck/ulockcheck_pid ]]; then
        if [[ "$1" -gt "0" ]]; then
            echo $1 > /proc/sysak/ulockcheck/ulockcheck_pid || echo "ulockcheck enable,pid:$1\n"
        fi
    fi
}

disaable_ulockcheck(){
    if [[ -f /proc/sysak/ulockcheck/enable ]]; then
        echo 0 > /proc/sysak/ulockcheck/enable
    fi
}

show_ulockcheck_thread(){
    cat /proc/sysak/ulockcheck/ulockcheck_pid | grep $1
}

show_ulockcheck_topfive(){
    cat /proc/sysak/ulockcheck/ulockcheck_pid >> $1
    cat $1 | grep owner -A 1
    echo "top 5:"
    cat $1 | awk -F "," '{print$3,$1,$2}' | grep futex | sort -r -t "[" -k 2 -n | head -n 5
}

parase_calltrace(){
    if [[ ! -z $LOG_DIR ]]; then

        mkdir -p $LOG_DIR
    fi

    if [[ ! -f $1 ]]; then
        echo "$1 dose not exist, use bbox print log"
        if [[ -f $SYSAK_BBOX ]];then
            cat $SYSAK_BBOX >> $LOG_DIR/$RAW_CALLTRACE_LOG
            CALLTRACE=$LOG_DIR/$RAW_CALLTRACE_LOG
        fi
    else
        CALLTRACE=$1
    fi
    pid=`cat $CALLTRACE | head -n2 | grep pid | awk -F "[" '{print$2}'| awk -F "]" '{print$1}'`

    for line in `cat $CALLTRACE`
    do
        if [[ $line =~ "task" ]];then
            echo "$line" >> $LOG_DIR/$CALLTRACE_LOG
        else
            if [[ $line =~ "0x" ]];then
                echo "$line" | awk '{print $2}'|xargs -I {} gdb -q --batch  -ex "x /8 {}" --pid $pid  | grep ">" | awk -F ":" '{print$1}' >> $LOG_DIR/$CALLTRACE_LOG
            fi
        fi
    done
}

enable_printstack(){
    echo $1 > /proc/sysak/ulockcheck/enable_print_ustack
}

set_wait_thresold(){
    if [[ "$1" -gt "0" ]]; then
        echo $1 > /proc/sysak/ulockcheck/wait_delaythresold
    fi
}

set_lock_thresold(){
    if [[ "$1" -gt "0" ]]; then
        echo $1 > /proc/sysak/ulockcheck/lock_delaythresold
    fi
}

set -- $(getopt -q p:hl:w:g:a:s:t:d "$@")

while [ -n "$1" ]
do
    case "$1" in
    -h) usage
        exit 0
        ;;
    -p) [[ -z $2 ]] && usage && continue
        PID=$2
        let LEN=${#PID}-2
        PID=${PID:1:$LEN}
        enable_ulockcheck $PID
        shift
        ;;
    -t) [[ -z $2 ]] && usage && break
        VAL=$2
        let LEN=${#VAL}-2
        VAL=${VAL:1:$LEN}
        enable_printstack $VAL
        exit 0
        ;;
    -l) [[ -z $2 ]] && usage && break
        THREASOLD=$2
        let LEN=${#THREASOLD}-2
        THREASOLD=${THREASOLD:1:$LEN}
        set_lock_thresold $THREASOLD
        exit 0
        ;;
    -w) [[ -z $2 ]] && usage && break
        THREASOLD=$2
        let LEN=${#THREASOLD}-2
        THREASOLD=${THREASOLD:1:$LEN}
        set_wait_thresold $THREASOLD
        exit 0
        ;;
    -a)  [[ -z $2 ]] && usage && break
        LOG_FILE=$2
        let LEN=${#LOG_FILE}-2
        LOG_FILE=${LOG_FILE:1:$LEN}
        show_ulockcheck_topfive $LOG_FILE
        exit 0
        ;;
    -g)  [[ -z $2 ]] && usage && break
        CALLTRACE_FILE=$2
        let LEN=${#CALLTRACE_FILE}-2
        CALLTRACE_FILE=${CALLTRACE_FILE:1:$LEN}
        parase_calltrace $CALLTRACE_FILE
        exit 0
        ;;
    -s) [[ -z $2 ]] && usage && break
        PID=$2
        let LEN=${#PID}-2
        PID=${PID:1:$LEN}
        show_ulockcheck_thread $PID
        exit 0
        ;;
    -d) echo "disable ulockcheck"
        disaable_ulockcheck
        exit 0
        ;;
    *)  echo "lack option"
        usage
        exit 1
        ;;
    esac
    shift
done
