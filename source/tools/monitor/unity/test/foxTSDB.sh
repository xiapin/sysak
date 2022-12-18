#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/
bash nativeFoxFFI.sh
luajit tsdbTime.lua
