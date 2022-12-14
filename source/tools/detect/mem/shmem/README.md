# 功能说明
检测shmem泄漏时对应的使用进程
# 使用
使用sysak shmem -h可查看所有可用的参数
 -h   帮助信息
 -t   诊断某个时间段的shmem对应的使用进程
for example:
shmem.py -t 5
# 结果分析
#sysak shmem -t 3   输出设置的时长内，存在使用量变化的共享内存所对应的进程。
shmem key      pid            name                total size(bytes)    increase(bytes)
1af40000       16955          td_connector        1073741824            30000
1af40000       22997          iohub-pcie-poll     1073741824            30000
1af41689       24838          iohub-bridge        33554432              1000
1af40000       24838          iohub-bridge        1073741824            30000
1af40000       30602          iohub-ctrl          1073741824            30000
1af40000       41045          iohub-seu           1073741824            30000
1af40000       47034          iohub-ctrl          1073741824            30000
1af41689       47142          iohub-fwd           33554432              1000
1af40000       47142          iohub-fwd           1073741824            30000
1af41689       63798          dpdkavs             33554432              1000
1af40000       63798          dpdkavs             1073741824            30000
1af40000       91424          agent_hook          1073741824            30000
1af40000       92437          iohub-ctrl          1073741824            30000

