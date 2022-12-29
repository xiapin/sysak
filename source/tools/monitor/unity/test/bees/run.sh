#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../collector/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../beaver/
source /etc/profile

cd ../../beaver
make
if [ $? -ne 0 ];then
	echo " make  -- Failed  : "$?
	exit 0
fi
cd -

cd ../../beeQ
make clean
make
if [ $? -ne 0 ];then
	echo " make  -- Failed  : "$?
	exit 0
fi

./bees
