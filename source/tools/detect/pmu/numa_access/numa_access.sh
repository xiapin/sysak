#!/bin/sh
#****************************************************************#
# ScriptName: numa_access.sh
# Author: lawrence-zx@alibaba-inc.com
# Create Date: 2022-06-16 19:42
# Modify Author: lawrence-zx@alibaba-inc.com
# Modify Date: 2022-06-16 19:42
# Function: 
#***************************************************************#

usage(){
	echo "sysak numa_access: show numa access information"
	echo "options: -h help information"
	echo "         -p pid, specify the pid"
	echo "         -c cpu, specify the cpu"
	echo "         -i interval, the interval checking the numa access times"
}

pid="all"
cpu="all"
interval="1"
pidArg="-a"
cpuArg=""

instructions_event="instructions"
cycles_event="cycles"
# OFFCORE_RESPONSE_0:L3_MISS_LOCAL.SNP_ANY
l3_miss_local_event="cpu/config=0x5301b7,config1=0x3f840085b7,name=l3_miss_local/"
# OFFCORE_RESPONSE_1:L3_MISS_REMOTE_(HOP0,HOP1,HOP2P)_DRAM.SNP_ANY
l3_miss_remote_event="cpu/config=0x5301bb,config1=0x3fb80085b7,name=l3_miss_remote/"

numa_access() {
	echo "Counting numa access times... Hit Ctrl-C to end."
	echo "pid: $pid, cpu: $cpu, interval: $interval"

	# print results to a new line when SIGINT is captured
	trap 'echo ""' 2
	perfcmd="perf stat $pidArg $cpuArg -e $cycles_event,$instructions_event,$l3_miss_local_event,$l3_miss_remote_event sleep $interval 2>&1"
	perfstat=$(eval $perfcmd)
	IFS=$'\n'
	for line in $perfstat; do
		if [[ $line =~ "instructions" ]]; then
			instructions=`echo $line | awk '{print $1}' | sed 's/,//g'`
		fi
		if [[ $line =~ "cycles" ]]; then
			cycles=`echo $line | awk '{print $1}' | sed 's/,//g'`
		fi
		if [[ $line =~ "l3_miss_local" ]]; then
			l3_miss_local=`echo $line | awk '{print $1}' | sed 's/,//g'`
		fi
		if [[ $line =~ "l3_miss_remote" ]]; then
			l3_miss_remote=`echo $line | awk '{print $1}' | sed 's/,//g'`
		fi
	done
	if [ -z "$instructions" ] || [ -z "$cycles" ] || [ -z "$l3_miss_local" ] || [ -z "$l3_miss_remote" ]; then
		echo "Command fail: $perfcmd"
		exit -1
	fi
	IPC=$(awk "BEGIN {printf \"%.2f\",$instructions/$cycles}")
	RDARate=$(awk "BEGIN {printf \"%.2f\",$l3_miss_remote/($l3_miss_local + $l3_miss_remote)}")
	
	printf "%-20s%-20s%-20s%-20s%-20s%-20s\n" "IPC" "Instructions" "Cycles" "Local-Dram-Access" "Remote-Dram-Access" "RDA-Rate"
	printf "%-20s%'-20.f%'-20.f%'-20.f%'-20.f%-20s\n" "$IPC" $instructions $cycles $l3_miss_local $l3_miss_remote $RDARate
}

while getopts 'hp:c:i:' OPT; do
        case $OPT in
                "h")
                        usage
                        exit 0
                        ;;
                "p")
			pid=$OPTARG
                        pidArg="-p $OPTARG"
			;;
                "c")
			maxcpu=`lscpu | grep "On-line" | awk -F- '{print $3}'`
			if [ "$OPTARG" -ge 0 ] 2>/dev/null; then
				if [ "$OPTARG" -gt "$maxcpu" ]; then
					echo cpu is not valid
					exit -1
			        fi
			else
			        echo cpu is not valid
			        exit -1
			fi
                        cpu=$OPTARG
			cpuArg="-C $OPTARG"
                        ;;
                "i")
                        interval="$OPTARG"
                        ;;
                *)
                        usage
                        exit -1
                ;;
        esac
done

numa_access

