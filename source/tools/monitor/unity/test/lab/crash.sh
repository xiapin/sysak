#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../../collector/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./native
source /etc/profile

cd native

make

cd -

make

if [ $? -ne 0 ];then
	echo " make  -- Faile  : "$?
	exit 0
fi

./crash
