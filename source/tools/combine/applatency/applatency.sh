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
irqlog=${log_dir}irqoff-`date "+%Y-%m-%d-%H-%M-%S"`.log
noschedlog=${log_dir}nosched-`date "+%Y-%m-%d-%H-%M-%S"`.log
sslowlog=${log_dir}sslow-`date "+%Y-%m-%d-%H-%M-%S"`.log
rqlog=${log_dir}rq-`date "+%Y-%m-%d-%H-%M-%S"`.log

lat_thresh=10
system_wide="true"

while getopts 't:l:c:p:h' OPT; do
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
			app_name=$cmdname
			system_wide="false"
			;;
		"p")
			pid=$OPTARG
			app_name=`head -n 1 /proc/$pid/status  | awk {'printf $2'}`
			system_wide="false"
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
$TOOLS_ROOT/nosched -f $noschedlog -t $lat_thresh &
nosched_pid=$!
if [ -n "$cmdname" ]; then
	$TOOLS_ROOT/runqslower -f $rqlog -P $lat_thresh &
	rqslow_pid=$!
	$TOOLS_ROOT/syscall_slow -t $lat_thresh -f $sslowlog -c $cmdname &
	sslow_pid=$!
else
	if [ -n "$pid" ]; then
		$TOOLS_ROOT/runqslower -f $rqlog -P -p $pid $lat_thresh &
		rqslow_pid=$!
		$TOOLS_ROOT/syscall_slow -t $lat_thresh -f $sslowlog -p $pid &
		sslow_pid=$!
	else
		$TOOLS_ROOT/runqslower -f $rqlog -P $lat_thresh &
		rqslow_pid=$!
		$TOOLS_ROOT/syscall_slow -t $lat_thresh -f $sslowlog &
		sslow_pid=$!
	fi
fi

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

check_irqoff() {
	irqoff_cnt=0
	if [ $system_wide == "true" ]; then
		irqoff_cnt=`grep -nr "20[0-9][0-9]-[0-1][0-9]-[0-3][0-9]" $irqlog | wc -l`
	else
		irqoff_cnt=`grep -nrw $app_name $irqlog | wc -l`
	fi

	if [ "$irqoff_cnt" -gt 0 ]; then
		echo "$irqoff_cnt irqoff events may delay $app_name running, see more $irqlog"
	fi
}

check_nosched() {
	nosched_cnt=0
	if [ $system_wide == "true" ]; then
		nosched_cnt=`grep -nr "20[0-9][0-9]-[0-1][0-9]-[0-3][0-9]" $noschedlog | wc -l`
	else
		nosched_cnt=`grep -nrw $app_name $noschedlog | wc -l`
	fi

	if [ "$nosched_cnt" -gt 0 ]; then
		echo "$nosched_cnt nosched events may delay $app_name running, see more $noschedlog"
	fi
}

check_rqslow() {
	rqslow_cnt=0
	if [ $system_wide == "true" ]; then
		rqslow_cnt=`grep -nr "20[0-9][0-9]-[0-1][0-9]-[0-3][0-9]" $rqlog | wc -l`
	else
		rqslow_cnt=`grep -nrw $app_name $rqlog | wc -l`
	fi

	if [ "$rqslow_cnt" -gt 0 ]; then
		echo "$rqslow_cnt runq slow events may delay $app_name running, see more $rqlog"
	fi

}

check_syscall() {
	slowsys_cnt=0
	if [ $system_wide == "true" ]; then
		slowsys_cnt=`grep -nr "20[0-9][0-9]-[0-1][0-9]-[0-3][0-9]" $sslowlog | wc -l`
	else
		slowsys_cnt=`grep -nrw \($app_name\) $sslowlog | wc -l`
	fi

	if [ "$slowsys_cnt" -gt 0 ]; then
		echo "$slowsys_cnt slow syscall events may delay $app_name running, see more $sslowlog"
	fi

}

check_irqoff
check_nosched
check_rqslow
check_syscall
