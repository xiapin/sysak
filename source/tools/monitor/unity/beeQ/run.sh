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

yaml_path=$1
[ ! $yaml_path ] && yaml_path="/etc/sysak/plugin.yaml"

echo $yaml_yaml_path
./unity-mon $yaml_path
