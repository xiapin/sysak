#!/bin/bash

yum install -y cmake3 wget \
               ncurses-devel zlib-devel \
               zlib-static glibc-static libstdc++-static

WORKDIR=/opt/work
GITHUB_PROXY=https://ghproxy.com
mkdir -p $WORKDIR

cd $WORKDIR
wget -c -O log4cpp-1.1.3.tar.gz  https://sourceforge.net/projects/log4cpp/files/log4cpp-1.1.x%20%28new%29/log4cpp-1.1/log4cpp-1.1.3.tar.gz/download
tar -xzf log4cpp-1.1.3.tar.gz -C ./
cd log4cpp && ./configure && make -j && make install

cd $WORKDIR
git clone ${GITHUB_PROXY}/https://github.com/Tencent/rapidjson.git
cd rapidjson
git submodule update --init
mkdir -p build && cd build && cmake3 .. && make -j && make install

cd $WORKDIR
git clone ${GITHUB_PROXY}/https://github.com/CLIUtils/CLI11.git
cd CLI11
git checkout 34c4310d9907f6a6c2eb5322fa7472474800577c
git submodule update --init
mkdir -p build && cd build && cmake3 .. && make -j && make install

cd $WORKDIR
wget -c https://invisible-mirror.net/archives/ncurses/current/ncurses-6.3-20220205.tgz
tar -zxf ncurses-6.3-20220205.tgz
cd ncurses-6.3-20220205/
./configure && make -j && make install
ln -sf /usr/lib/libncurses.a /usr/lib/libtinfo.a

