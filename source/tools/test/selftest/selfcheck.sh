selfcheck_dir=/var/log/sysak/selfcheck/
baselog=${selfcheck_dir}base.log
checklog=${selfcheck_dir}check.log
wait_done="false"

save_sys_info() {
	echo > $baselog

	echo "kernel log:" >> $baselog
	dmesg | tail -n 1 >> $baselog
	
	echo "cpuinfo:" >> $baselog
	mpstat $cpuarg 1 1 | awk 'END {print $3" "$5" "$6" "$7" "$8" "$12}' >> $baselog

	echo "meminfo:" >> $baselog
	cat /proc/meminfo | grep MemAvailable >> $baselog
}

check_exception() {
	kernel_log=`cat $baselog | grep "kernel log:" -A 1 | grep -v "kernel log:"`
	new_log=dmesg | grep "\$kernel_log" -A 99999 | grep -v "\$kernel_log"
	echo "kernel log:" >> $checklog
	echo $new_log >> $checklog
	#check if bug \warning\ error\ trace\ rcustall\ lockup etc
	echo $new_log | grep -E "BUG|WARNING|LOCKUP|stall|hung_task" > /dev/null
	if [ $? == 0 ]; then
		echo "kernel error happened, check failed"
		exit
	fi
}

check_cpu_cost() {
	cpu_log=`cat $baselog | grep "cpuinfo:" -A 1 | grep -v "cpuinfo:"`
	cpu_idle_before=`echo $cpu_log | awk '{print $6}'`

	new_cpu=`mpstat $cpuarg 1 1 | awk 'END {print $3" "$5" "$6" "$7" "$8" "$12}'`
	echo "cpuinfo:" >> $checklog
	echo $new_cpu >> $checklog
	
	cpu_idle=`echo $new_cpu | awk '{print $6}'`
	if [ $(echo "$cpu_idle + 3 < $cpu_idle_before" | bc) -eq 1 ] ;then
		echo "cpu cost too much, check failed"
		exit 1
	fi
}

check_mem_cost() {
	mem_log=`cat $baselog | grep "meminfo:" -A 1 | grep -v "meminfo:"`
	avail_mem_before=`echo $mem_log | awk '{print $2}'`
	
	new_meminfo=`cat /proc/meminfo | grep MemAvailable`
	echo "meminfo:" >> $checklog
	echo $new_meminfo >> $checklog

	avail_mem=`echo $new_meminfo | awk '{print $2}'`
	if [ $(echo "$avail_mem + 51200 < $avail_mem_before" | bc) -eq 1 ] ;then
		echo "mem cost too much, check failed"
		exit 1
	fi
}

check_kernel_runlantency() {
	sysak runlantency -e
}

lkm_base_check() {
	SYSAK=$(whereis sysak| awk '{print $2}')
	if [ -z $SYSAK ];then
		echo sysak not installed
		exit 1
	fi
	local SYSAK_PATH=$(dirname $SYSAK)
	local LIB_PATH=$SYSAK_PATH/.sysak_compoents/lib/`uname -r`
	local SYSAK_MOD=$LIB_PATH/sysak.ko
	echo "doing sysak module base check"
	save_sys_info
	echo "load sysak.ko"
	insmod $SYSAK_MOD
	echo > $checklog
	check_exception
	echo "check kernel exception pass"
	check_cpu_cost
	echo "check cpu cost pass"
	check_mem_cost
	echo "check mem cost pass"
	echo "unload sysak.ko"
	rmmod sysak
	check_exception
	echo "after unload, check kernel exception pass"
}

tool_running_check() {
	echo "doing sysak tool check"
	save_sys_info
	if [ $wait_done = true ]; then
		echo "excute sysak ${*}" 
		sysak ${*}
	else
		echo "excute sysak ${*} backgroud"
		sysak ${*} &
	fi
	echo > $checklog
	check_exception
	echo "check kernel exception pass"
	check_cpu_cost
	echo "check cpu cost pass"
	check_mem_cost
	echo "check mem cost pass"
}

mkdir -p $selfcheck_dir

usage() {
	echo "selfcheck, do the base test for sysak compoents"
	echo "Usage: selfcheck [-m] | [[-a] -C subcmd]"
	echo "       -m, check linux kernel module"
	echo "       -C subcmd, check the subcmd, like memleak etc"
	echo "       -a, do check after the subcmd done"
	echo "           if not set, means check it running"
}

while getopts 'C:amh' OPT; do
	case $OPT in
		"h")
			usage
			exit 0
			;;
		"m")
			lkm_base_check
			exit 0
			;;
		"a")
			wait_done="true"
			;;
		"C")
			cmdstart=$((OPTIND-1))
			cmd=${*:$cmdstart}
			tool_running_check $cmd
			exit 0
			;;
		*)
			usage
			exit -1
		;;
	esac
done
