#!/bin/sh
#****************************************************************#
# ScriptName: applatency.sh
# Author: $SHTERM_REAL_USER@alibaba-inc.com
# Create Date: 2023-01-03 10:53
# Modify Author: $SHTERM_REAL_USER@alibaba-inc.com
# Modify Date: 2023-01-03 10:53
# Function:
#***************************************************************#
TOOLS_ROOT="$SYSAK_WORK_PATH/tools"
usage() {
	echo "sysak applatency: auto detect the latency source"
	echo "options: -h,          help information"
	echo "         -t threshold,latency time, default 10ms"
	echo "         -l logfile,  file for tracing log"
	echo "         -c name,     the traced process name"
	echo "         -p pid,      the traced process id"
}


log_dir=/var/log/sysak/applatency/
irqlog=${loadtask_dir}irqoff-`date "+%Y-%m-%d-%H-%M-%S"`.log
noschedlog=${loadtask_dir}nosched-`date "+%Y-%m-%d-%H-%M-%S"`.log
sslowlog=${loadtask_dir}sslow-`date "+%Y-%m-%d-%H-%M-%S"`.log
rqlog=${loadtask_dir}rq-`date "+%Y-%m-%d-%H-%M-%S"`.log

lat_thresh=10

while getopts 't:l:c:ph' OPT; do
	case $OPT in
		"h")
			usage
			exit 0
			;;
		"t")
			lat_thresh=$OPTARG
			;;
		"l")
			irqlog=irqoff-$OPTARG
			noschedlog=nosched-$OPTARG
			sslowlog=sslow-$OPTARG
			rqlog=rq-$OPTARG
			;;
		"c")
			cmdname="$OPTARG"
			;;
		"p")
			pid=$OPTARG
			;;
		*)
			usage
			exit -1
		;;
	esac
done

mkdir -p $log_dir

$TOOLS_ROOT/irqoff -f $irqlog -t $lat_thresh &
irqoff_pid=$!
echo "irqoff trace started($irqoff_pid)"
$TOOLS_ROOT/nosched -f $noschedlog -t $lat_thresh &
nosched_pid=$!
echo "nosched trace started($nosched_pid)"
if [ -n "$cmdname" ]; then
	process_arg="-c $cmdname"
else
	if [ -n "$pid" ]; then
		process_arg="-p $pid"
		$TOOLS_ROOT/runqslower -f $rqlog -P $process_arg $lat_thresh &
		rqslow_pid=$!
		echo "runq slow trace started($rqslow_pid)"
	fi
fi
$TOOLS_ROOT/syscall_slow -t $lat_thresh -f sslowlog $process_arg &
sslow_pid=$!
echo "syscall slow trace started($sslow_pid)"

cleanup() {
	kill $irqoff_pid
	kill $nosched_pid
	kill $sslow_pid
	kill $rqslow_pid
}

trap cleanup INT QUIT TERM
wait $irqoff_pid
wait $nosched_pid
wait $sslow_pid
wait $rqslow_pid
