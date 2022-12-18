#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../collector/native/
./nativeProcFFI.sh
luajit collectorProc.lua
