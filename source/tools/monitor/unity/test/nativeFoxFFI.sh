#!/bin/bash

cd ../tsdb/native/
source /opt/rh/devtoolset-9/enable
make
if [ $? -ne 0 ];then
	echo " make  -- Faile  : "$?
	exit 0
fi
cd -
luajit nativeFoxFFI.lua