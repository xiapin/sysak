# 功能说明
监控管理一键执行工具

使用说明:
```
Usage: sysak monctl 
Options:
    -e         run monctl
    -h			help info
Usage step:
1. enable mod in /usr/log/sysak/sysak_monctl.conf
2. run sysak monctl, example: sysak monctl -e &
```

/usr/log/sysak/sysak_monctl.conf
```
####[module]
mod_iomonitor off
mod_mservice off
mod_oomkill off
mod_raptor off
#mod_unity on

####[cmdline]
cmd_iomonitor sysak -h
cmd_mservice sysak loadtask -h
cmd_oomkill sysak oomkill -h
cmd_raptor sysak list
#cmd_unity sysak list -a
```
若要使能，需要将off改成on，然后monctl会依次执行cmdline中设置的命令