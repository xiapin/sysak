#!/bin/bash

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:./lib/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../tsdb/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../collector/native/
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:../beaver/

if [ ! -f "/lib64/libyaml-0.so.2" ]; then
  echo "install libyaml."
  cp ../../install/libyaml-0.so* /lib64
  cp ../../install/libyaml.so* /lib64
  cp ../../install/libluajit-5.1.so* /lib64
fi

if [ ! -d "/usr/local/lib/lua/5.1/" ]; then
  echo "install libs."
  mkdir -p /usr/local/lib/lua/5.1/
  cp -r ../../lib/* /usr/local/lib/lua/5.1/
fi

if [ ! -d "/usr/local/share/lua/5.1/" ]; then
  echo "install luas."
  mkdir -p /usr/local/share/lua/5.1/
  cp -r ../../lua/* /usr/local/share/lua/5.1/
fi

./unity-mon
