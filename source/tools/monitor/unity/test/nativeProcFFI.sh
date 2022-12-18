#!/bin/bash

source /etc/profile
cd ../collector/native/
make
if [ $? -ne 0 ];then
	echo " make  -- Faile  : "$?
	exit 0
fi
cd -
luajit nativeProcFFI.lua
