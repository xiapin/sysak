#!/bin/bash

DIR=$( cd "$(dirname "${BASH_SOURCE[0]}")" && pwd);
echo $DIR
cd $DIR
ulimit -c unlimited

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../collector/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../beaver/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../../install/

export LUA_PATH="../../lua/?.lua;../../lua/?/init.lua;"
export LUA_CPATH="./lib/?.so;../../lib/?.so;../../lib/loadall.so;"

export SYSAK_WORK_PATH="/usr/local/sysak/.sysak_components"

yaml_path=$1
[ ! $yaml_path ] && yaml_path="/etc/sysak/base.yaml"

#download sysak.ko
#sysak -oss -d

echo $yaml_yaml_path
./unity-mon $yaml_path
