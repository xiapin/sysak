#!/bin/sh
#****************************************************************#
# ScriptName: iosdiag.sh
# Author: guangshui.lgs@alibaba-inc.com
# Create Date: 2021-07-02 11:44
# Modify Author: $SHTERM_REAL_USER@alibaba-inc.com
# Modify Date: 2021-07-02 11:45
# Function: 
#***************************************************************#
if [ "$SYSAK_WORK_PATH" != "" ]; then
WORK_PATH=$SYSAK_WORK_PATH
else
WORK_PATH=/usr/local/sbin/.sysak_components
fi
TOOLS_PATH=$WORK_PATH/tools/`uname -r`
LIB_PATH=$WORK_PATH/lib/`uname -r`
latency_bin=$WORK_PATH/tools/latency
iolatency_bin=$WORK_PATH/tools/tracing_latency
hangdetect_bin=$WORK_PATH/tools/hangdetect
data_analysis=$WORK_PATH/tools/iosdiag_data_analysis
threshold_arg="-t 1000"
kernel_version=$(uname -r)
config_file="/boot/config-$kernel_version"

function usage() {
	echo "Usage: sysak iosdiag [options] subcmd [cmdargs]"
	echo "       subcmd:"
	echo "              latency, io latency diagnosis"
	echo "              hangdetect, io hang diagnosis"
	echo "       cmdargs:"
	echo "              -h, help info"
	echo "       options:"
	echo "              -u url, transfer datafile to remote url"
	echo "              -s latency|[hangdetect], stop diagnosis"
}

check_bpf_config() {
    if grep -q "CONFIG_BPF=y" "$config_file"; then
        # echo "found CONFIG_BPF=y"
		support_bpf="Yes"
    else
        # echo "CONFIG_BPF=y not found"
		support_bpf="No"
	fi
}

upload_data() {
	datapath=$(dirname $logfile)
	cd $datapath
	tar -zcf iosdiag_$1.tar.gz ./result.log*
	curl -i -q  -X PUT -T iosdiag_$1.tar.gz $url
	rm -f iosdiag_$1.tar.gz
}

datafile_analysis() {
	if [ -e "$logfile" ]
	then
		run_python="python"
		if [ -e "/usr/bin/python" ]; then
			run_python="/usr/bin/python"
		elif [ -e "/usr/bin/python2" ]; then
			run_python="/usr/bin/python2"
		elif [ -e "/usr/bin/python2.7" ]; then
			run_python="/usr/bin/python2.7"
		elif [ -e "/usr/bin/python3" ]; then
			run_python="/usr/bin/python3"
		elif [ -e "/usr/bin/python3.10" ]; then
			run_python="/usr/bin/python3.10"
		elif [ -e "/usr/bin/python3.5" ]; then
			run_python="/usr/bin/python3.5"
		fi
		if [ -n "$offline" ]; then
			# echo "offline mode, no data analysis"
			echo $threshold >> $logfile
		else
			# echo "iosdiag datafile analysis starting..."
			$run_python $data_analysis --$1 -s -f $logfile $threshold_arg
		fi
	fi
}

hang_mod_depend()
{
	res=`lsmod | grep sysak`
	if [ -z "$res" ]; then
		insmod $LIB_PATH/sysak.ko
		if [ $? -ne 0 ]; then
			echo "insmod ko failed, please check the ko files."
			exit $?
		fi
	fi
}

enable_hangdetect() {
	if [ ! -e "$hangdetect_bin" ]; then
		echo "$hangdetect_bin not found"
		echo "iosdiag hangdetect not support '$(uname -r)', please report to the developer"
		exit -1
	fi
	{
		flock -n 3
		[ $? -eq 1 ] && { echo "another hangdetect is running."; exit -1; }
		trap disable_hangdetect SIGINT SIGTERM SIGQUIT
		#mkdir -p `dirname $datafile`
		hang_mod_depend
		chmod +x $hangdetect_bin
		rm -f $(dirname $logfile)/result.log*
		$hangdetect_bin $* &
		hangdetect_pid=$!
		wait $hangdetect_pid
		disable_hangdetect
	} 3<> /tmp/hangdetect.lock
}

disable_hangdetect() {
	pid=$hangdetect_pid
	if [ $diag_stop ]; then
		pid=`ps -ef | grep "\$hangdetect_bin" | grep -v "grep" | awk '{print $2}'`
	fi

	comm=`cat /proc/$pid/comm 2>/dev/null`
	if [ "$comm" = "hangdetect" ]
	then
		kill -9 $pid 2>/dev/null
	fi

	res=`lsmod | grep sysak`
	if [ ! -z "$res" ]; then
		rmmod sysak
	fi

	if [ ! $diag_stop ]; then
		datafile_analysis hangdetect
		if [ -n "$url" ]; then
			upload_data hangdetect
		fi
	fi
	exit 0
}

enable_latency() {
	check_bpf_config
    args=("$@")
    num_args=${#args[@]}
    if [ "$support_bpf" = "Yes" ]; then
        if [ ! -e "$latency_bin" ]; then
            echo "$latency_bin not found"
            echo "iosdiag latency not support '$(uname -r)', please report to the developer"
            exit -1
        fi
    else
        if [ ! -e "$iolatency_bin" ]; then
            echo "$iolatency_bin not found"
            echo "iosdiag iolatency not support '$(uname -r)', please report to the developer"
            exit -1
        fi
		latency_bin=$iolatency_bin
    	if (( $num_args % 2 == 1 && $num_args > 1 )); then
		    last_arg=${args[$num_args-1]}  
        	unset args[num_args-1]  
        	args+=("-d" "$last_arg")   
    	fi
    fi

	# echo ${args[@]}
    {
        flock -n 3
        [ $? -eq 1 ] && { echo "another latency is running."; exit -1; }
        trap disable_latency SIGINT SIGTERM SIGQUIT
        chmod +x $latency_bin
        rm -f $(dirname $logfile)/result.log*
		$latency_bin ${args[@]} &
        latency_pid=$!
        wait $latency_pid
        disable_latency
    } 3<> /tmp/latency.lock
}

disable_latency() {
	pid=$latency_pid
	if [ $diag_stop ]; then
		pid=`ps -ef | grep "\$latency_bin" | grep -v "grep" | awk '{print $2}'`
	fi

	comm=`cat /proc/$pid/comm 2>/dev/null`
	if [ "$comm" = "latency" ]
	then
		kill -9 $pid 2>/dev/null
	fi

	if [ ! $diag_stop ]; then
		datafile_analysis latency
		if [ -n "$url" ]; then
			upload_data latency
		fi
	fi
	exit 0
}

#execute command,every command need such args:
# -h/--help: command usage
# -f/--file: output files, default stdout
#            output format jason
# -d/--disable
function execute() {
	threshold=$(echo "$*"|awk -F "-t" '{print $2}'|awk '{print $1}')
	[ "$threshold" != "" ] && { threshold_arg="-t $threshold"; }
	logd=$(echo "$*"|awk -F "-f" '{print $2}'|awk '{print $1}')
	[ "$logd" != "" ] && { logfile=$logd/result.log.seq; }
	#echo cmd:$1 ${*:2}
	enable_$1 ${*:2}
}

diag_stop=
while getopts 'hos:u:' OPT; do
	case $OPT in
		"u")
			url=$OPTARG
			;;
		"s")
			diag_stop=true
			subcmd=$OPTARG
			;;
		"o")
			offline="offline"
			;;
		*)
			usage
			exit 0
			;;
	esac
done

if [ $diag_stop ]; then
	echo "disable $subcmd"
	disable_$subcmd
	exit 0
fi

subcmd=${@:$OPTIND:1}
subargs=${*:$OPTIND+1};
[[ "$subcmd" != "latency" && "$subcmd" != "hangdetect" ]] && { echo "not support subcmd $subcmd!!!"; usage; exit -1; }
logfile="/var/log/sysak/iosdiag/$subcmd/result.log.seq"
execute $subcmd $subargs

