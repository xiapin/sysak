# 1、搭建虚拟环境
    pip3.9 uninstall virtualenv -y  卸载3.9版本的virtualenv
    pip3.6 install virtualenv -i "${ALIYUN_MIRROR}"  安装virtualenv
    -i 指定镜像源
   进入项目根目录，执行：virtualenv virtualenv

# 2、requirement配置
   将所需的包全部列在requirement.txt 参考：https://code.alibaba-inc.com/SystemTools/sysAK/blob/opensource_branch/source/tools/combine/sar/requirements.txt
   
# 3、调整venv.sh 参数
   调整venv.sh 脚本，修改venv.sh中的参数50行执行命令
    两种打包方式:
        1）pyinstaller -F  sample.py --add-data './config.yaml:./' -y
        2）写一个pyinstaller的spec文件：pyinstaller spec.spec

# 4、打包
   执行venv.sh