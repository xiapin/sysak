#!/bin/bash

pkill unity-mon

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib/
#export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../lib/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../collector/lib/
source /etc/profile

cd ../../beeQ || exit 1
[ ! -d ../lib ] && mkdir ../lib
cp ./lib/*.so ../lib
cp ../tsdb/native/*.so ../lib
[ ! -d ../bin ] && mkdir ../bin
cp unity-mon ../bin

cd ../bin || exit 1

yaml_path=$1
[ ! $yaml_path ] && yaml_path="/etc/sysak/plugin.yaml"

echo $yaml_yaml_path
./unity-mon $yaml_path &
