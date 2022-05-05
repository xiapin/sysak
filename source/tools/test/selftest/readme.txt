此脚本用于sysAK的基础稳定性测试，不包括子功能的功能是否正常
测试2种场景：
1、sysAK模块的基础稳定性
测试方法:
1) 将此脚本放在任意目录
2) sudo ./selfcheck -m


2、单功能运行时的稳定性
测试方法:
1) 将此脚本放在任意目录
2) 测试单功能运行期间的副作用
   sudo ./selfcheck -C cmd cmdargs
   cmd和cmdargs为要测试的功能和参数，比如测试sysak loadtask -s这个命令:
       sudo ./selfcheck -C loadtask -s

3) 测试单功能运行后的副作用
    sudo ./selfcheck -a -C cmd cmdargs
   
