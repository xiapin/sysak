#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../collector/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../beaver/
source /etc/profile

cd ../../beeQ
make
if [ $? -ne 0 ];then
	echo " make  -- Failed  : "$?
	exit 0
fi

./bees
