#!/bin/bash

pkill unity-mon

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../collector/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../beaver/
source /etc/profile

cd ../../beeQ

yaml_path=$1
[ ! $yaml_path ] && yaml_path="/etc/sysak/plugin.yaml"

echo $yaml_yaml_path
./unity-mon $yaml_path &
