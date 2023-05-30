#!/usr/bin/python2
#coding:utf-8

import re
import os.path
import getopt
import sys
import time

list_app = {'systemd', 'syslog-ng', 'docker', 'systemd-network', 'dbus', 'polkit'}

SYSTEMD_NEW_VERSION_ALINUX = "systemd-219-73.3.al7.3.x86_64"
POLKIT_NEW_VERSION = "polkit-0.112-26.2.al7"

kern_log = "/var/log/kern"

result_dir = "/var/log/sysak/mmaptrace/"
trace_log = result_dir + "mmaptrace_log"
analysed_log = result_dir + "analysed_log"
status_log = result_dir + "status_log"
app_log = result_dir + "app_log"

proc_dir = "/proc/sysak/mmaptrace/"
mod_enable = proc_dir + "mmaptrace_enable"
trace_reslut = proc_dir + "mmaptrace_print"
monitor_pid = proc_dir + "mmaptrace_pid"
monitor_size = proc_dir + "mmaptrace_len"

gdb_bin = "/usr/bin/gdb"

MEM_THRESHOLD = 200*1024
HALF_CPU = 0.5
DETECT_DELAY = 5*60

name_skip = ""
check_time = ""
task_num = 5

def exectue_cmd(command):
    command=command.replace("\n", "")
    command_fd = os.popen(command, "r")
    ret = command_fd.read()
    command_fd.close()
    return ret
def print_trace_top3(log_path):
    if os.path.isfile(log_path):
        log_fd = open(log_path,"r")
        for line in log_fd.readlines():
            if "用户态堆栈4" in line:
                break
            print(line.split('\n'))
        log_fd.close

def show_result(trace_reslut):
    trace_fd = open(trace_reslut,"r")
    pid_fd = open(monitor_pid,"r")
    pid_v = 0

    for line_1 in pid_fd.readlines():
        pagetype_1 = re.search( r'pid:(\d+),*', line_1, re.I)
        if pagetype_1:
            pid_v = pagetype_1.group(1)

    if not os.path.exists(result_dir):
        os.mkdir(result_dir)
    log_fd = open(trace_log,"a+")
    analysed_fd = open(analysed_log,"a+")
    for line_2 in trace_fd.readlines():
        log_fd.writelines(line_2)
        pagetype_2 = re.search( r'#~ +(\S+) *', line_2, re.I)
        if pagetype_2:
            if not os.path.exists(gdb_bin):
                exectue_cmd("yum install gdb -y")
            command = 'gdb -q --batch  -ex "x /8 ' + pagetype_2.group(1) + ' " --pid ' + pid_v + ' | grep ">" | awk -F ":" \'{print$1}\''
            utrace = exectue_cmd(command)
            analysed_fd.writelines(utrace)
        else:
            analysed_fd.writelines(line_2)

    log_fd.writelines("------------------------------")
    analysed_fd.writelines("-------------------------")
    trace_fd.close()
    pid_fd.close()
    log_fd.close()
    print_trace_top3(analysed_log)

def monitor_status():
    if not os.path.exists(result_dir):
        os.mkdir(result_dir)

    app_dir = open(app_log,"a+")
    new_time = time.localtime()
    date = time.strftime("%Y-%m-%d %H:%M", new_time)
    date = date + '\n'
    for app_name in list_app:
        if app_name != "systemd":
            command_status = 'systemctl status ' + app_name + '| grep Active | awk -F " " \'{print $2$3}\''
            status = exectue_cmd(command_status)

        if app_name == "systemd":
           pid = "1"
        else:
            command_pid = 'systemctl status ' + app_name + '| grep "Main PID" |awk -F " " \'{print $3}\''
            pid = exectue_cmd(command_pid)

        if not pid:
            continue

        command_mem = 'cat /proc/' + pid + '/status | grep VmRSS | awk -F " " \'{print$2}\''
        mem_use_t1 = exectue_cmd(command_mem)

        command_vmem = 'cat /proc/' + str(pid) + '/status | grep VmSize | awk -F " " \'{print$2}\''
        vmem_use_t1 = exectue_cmd(command_vmem)

        command_cpu = 'pidstat -p ' + pid + ' | grep ' + app_name + '| awk -F " " \'{print $8}\''
        cpu = exectue_cmd(command_cpu)

        command_cpu_user = 'pidstat -p ' + pid + ' | grep ' + app_name + '| awk -F " " \'{print $5}\''
        cpu_user = exectue_cmd(command_cpu_user)

        command_cpu_sys = 'pidstat -p ' + pid + ' | grep ' + app_name + '| awk -F " " \'{print $6}\''
        cpu_sys = exectue_cmd(command_cpu_sys)
        app_dir.writelines(date)
        if app_name != "systemd":
            app_dir.writelines("%s %s %s %s %s %s %s\n" %(app_name, status, pid, mem_use_t1+"KB", cpu, cpu_user, cpu_sys))
        else:
            app_dir.writelines("%s %s %s   %s %s %s\n" %(app_name, pid, mem_use_t1+"KB", cpu, cpu_user, cpu_sys))
        if float(cpu) >= float(HALF_CPU):
            if float(cpu) >= float(2*HALF_CPU):
                if app_name == "systemd-network":
                    command_version = 'rpm -qa |grep systemd-2'
                    version = os.system(command_version)
                    print("%s:cpu使用率%s,超过100%%" %(app_name, cpu.replace("\n", "")))
                    print("systemd当前版本(%s)较低,建议升级到>=%s" %(version, SYSTEMD_NEW_VERSION_ALINUX))
                    continue
            else:
                print("%s:cpu使用率%s,超过50%%" %(app_name, cpu.replace("\n", "")))

        mem_use_t2 = exectue_cmd(command_mem)
        vmem_use_t2 = exectue_cmd(command_vmem)
        mem_inc = int(mem_use_t2) - int(mem_use_t1)
        vmem_inc = int(vmem_use_t2) - int(vmem_use_t1)

        time.sleep(DETECT_DELAY)
        if int(mem_use_t2) > int(MEM_THRESHOLD):
            print("%s:内存使用超过100M\n" %(app_name))
            print("5分钟RSS增长:%sKB\n" %(mem_inc))
            print("5分钟VM增长:%sKB\n" %(vmem_inc))
            print("可能存在泄漏，可使用-p选项抓取用户态调用栈")

            if app_name == "systemd":
                command_version = 'rpm -qa |grep systemd-2'
                version = exectue_cmd(command_version)
                if version < SYSTEMD_NEW_VERSION_ALINUX:
                    print("systemd当前版本(%s)较低,建议升级到>=%s" %(version.replace("\n", ""), SYSTEMD_NEW_VERSION_ALINUX))
                kernlog_fd = open(kern_log,"r")
                for line in kernlog_fd.readlines():
                    if line.find("Attempted to kill init!") != -1:
                        print("系统存在因为systemd异常退出，且panic过")
                kernlog_fd.close()

            if app_name == "polkit":
                command_version = 'rpm -qa |grep polkit-0'
                version = exectue_cmd(command_version)
                if version < POLKIT_NEW_VERSION:
                    print("%s当前版本(%s)较低,建议升级到>=%s" %(app_name, version.replace("\n", ""), POLKIT_NEW_VERSION))
    app_dir.close()

def quick_check():
    global name_skip
    skip_name_list = []
    tasks_list = []
    pid_path='/proc'

    if name_skip:
        skip_name_list = name_skip.split(',')

    pid_folders= os.listdir(pid_path)
    for file in pid_folders:
        if file.isdigit():
            pid_status = pid_path + "/" + file + "/status"
            if skip_name_list:
                pid_comm = pid_path + "/" + file + "/comm"
                if os.path.exists(pid_comm):
                    comm_fd = open(pid_comm,"r")
                    line_tmp = comm_fd.readlines()
                    fist_line = line_tmp[0].split('\n')[0]
                    comm_fd.close
                    if fist_line in skip_name_list:
                        continue
            if os.path.exists(pid_status):
                task_info = {}
                vmsize = "0"
                rsssize = "0"
                name = "NULL"
                status_fd = open(pid_status,"r")

                for line in status_fd.readlines():
                    vmszie_line = re.search( r'VmSize:(\s+)(\d+)(.*)', line, re.I)
                    if vmszie_line:
                        vmsize = vmszie_line.group(2)
                        continue
                    rssszie_line = re.search( r'VmRSS:(\s+)(\d+)(.*)', line, re.I)
                    if rssszie_line:
                        rsssize = rssszie_line.group(2)
                        continue
                    name_line = re.search( r'Name:(\s+)(.*)', line, re.I)
                    if name_line:
                        name = name_line.group(2)
                        continue
                status_fd.close
                task_info["vmsize"] = vmsize.zfill(9)
                task_info["rsssize"] = rsssize.zfill(9)
                task_info["name"] = name
                task_info["pid"] = file
                #print task_info
                tasks_list.append(task_info)
    #vm_sorted = sorted(tasks_list, key = lambda x:x['vmsize'],reverse=True)
    rss_sorted = sorted(tasks_list, key = lambda x:x['rsssize'],reverse=True)

    if check_time:
        count = 0
        print("please waiting %ds..." %(int(check_time)*task_num))
        while (count < task_num):

            time.sleep(int(check_time))
            pid_status = "/proc/" + rss_sorted[count]["pid"] + "/status"
            rss_sorted[count]["rss_inc"] = 0
            rss_sorted[count]["vm_inc"] = 0
            if os.path.exists(pid_status):
                new_vmsize = "0"
                new_rsssize = "0"
                status_fd = open(pid_status,"r")
                for line in status_fd.readlines():
                    vmszie_line = re.search( r'VmSize:(\s+)(\d+)(.*)', line, re.I)               
                    if vmszie_line:
                        new_vmsize = vmszie_line.group(2)
                        continue
                    rssszie_line = re.search( r'VmRSS:(\s+)(\d+)(.*)', line, re.I)
                    if rssszie_line:
                        new_rsssize = rssszie_line.group(2)
                        continue
                status_fd.close
                rss_sorted[count]["rss_inc"] = (int(new_rsssize) - int(rss_sorted[count]["rsssize"]))/int(check_time)
                rss_sorted[count]["vm_inc"] = (int(new_vmsize) - int(rss_sorted[count]["vmsize"]))/int(check_time)
            count += 1

    count = 0
    print("RSS size Top %d:" %(task_num))
    if check_time:
        print("%-15s%-20s%-15s%-15s%-15s%-15s" %("pid", "name", "rsssize(kb)", "rss_inc(kb)/s", "vmsize(kb)", "vm_inc(kb)/s"))
    else:
        print("%-15s%-20s%-15s%-15s" %("pid", "name", "rsssize(kb)", "vmsize(kb)"))
    while (count < task_num):
        if check_time:
            print ("%-15s%-20s%-15d%-15d%-15d%-15d" %(rss_sorted[count]["pid"], rss_sorted[count]["name"], int(rss_sorted[count]["rsssize"]), int(rss_sorted[count]["rss_inc"]), int(rss_sorted[count]["vmsize"]), int(rss_sorted[count]["vm_inc"])))
        else:
            print ("%-15s%-20s%-15d%-15d" %(rss_sorted[count]["pid"], rss_sorted[count]["name"], int(rss_sorted[count]["rsssize"]), int(rss_sorted[count]["vmsize"])))
        count += 1

def enable_kernelmod(enable):
    if not os.path.isfile(mod_enable):
        print("%s does not exist" %mod_enable)
        return
    mod_enable_fd = open(mod_enable,"w")
    if enable:
        mod_enable_fd.write("1")
    else:
        mod_enable_fd.write("0")
    mod_enable_fd.close()

def set_parameter(file, value):
    size_fd = open(file,"w")
    size_fd.write(str(value))
    size_fd.close()

def show(file):
    if not os.path.isfile(file):
        print("%s does not exist" %file)
        return
    fd = open(file,"r")
    str = fd.read()
    print(str)
    fd.close()

def usage():
    print ('mmaptrace: user mode memory leak monitor tool')
    print ('Usage: mmaptrace <option> [<args>]')
    print ('  -h                 help information')
    print ('  -c                 check main system services status')
    print ('  -C                 quickly check user memory leak and display top 5')
    print ('  -k                 task skip list with using -C')
    print ('  -t                 check time for each task')
    print ('  -n                 show task numbers, default is 5')
    print ('  -p <pid>           enable memory leak trace')
    print ('  -l                 set mmaloc/mmap size')
    print ('  -s                 show user mode trace')
    print ('  -d                 disable hang check')
    print ('example:')
    print ('sysak mmaptrace -C -k name1,name2 -t 2 -n 6')
    print ('sysak mmaptrace -p 111 -l 1024')
    return

opts,args = getopt.getopt(sys.argv[1:],'-h-c-k:-t:-n:-C-p:-s-l:-d')

def main():
    enable_quick_check = 0
    for opt_name,opt_value in opts:
        if opt_name in ('-h'):
            usage()
            sys.exit()
        if opt_name in ('-c'):
            monitor_status()
            sys.exit()
        if opt_name in ('-C'):
            enable_quick_check = 1
        if opt_name in ('-t'):
            if opt_value:
                global check_time
                check_time = opt_value
            else:
                usage()
                sys.exit()
        if opt_name in ('-n'):
            if opt_value:
                global task_num
                task_num = int(opt_value)
            else:
                usage()
                sys.exit()
        if opt_name in ('-k'):
            if opt_value:
                global name_skip
                name_skip = opt_value
            else:
                usage()
                sys.exit()
        if opt_name in ('-p'):
            if opt_value:
                pid = int(opt_value)
                enable_kernelmod(1)
                set_parameter(monitor_pid, pid)
            else:
                usage()
                sys.exit()
        if opt_name in ('-s'):
            show_result(trace_reslut)
            sys.exit()
        if opt_name in ('-l'):
            if opt_value:
                size = int(opt_value)
                set_parameter(monitor_size, size)
            else:
                usage()
            sys.exit()
        if opt_name in ('-d'):
            enable_kernelmod(0)
            print("disable kernel module")
            sys.exit()

    if enable_quick_check:
        quick_check()


if __name__ == '__main__':
    main()
