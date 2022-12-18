#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/

bash nativeFoxFFI.sh
if [ $? -ne 0 ];then
	echo " native api-- Failed  : "$?
	exit 0
fi

luajit tsdbRead.lua
