FROM centos:7
MAINTAINER "liaozhaoyan <zhaoyan.lzy@alibaba-inc.com>"
WORKDIR /root/
RUN yum install -y http://mirrors.tuna.tsinghua.edu.cn/centos/7/extras/x86_64/Packages/centos-release-scl-rh-2-3.el7.centos.noarch.rpm && \
    yum install -y devtoolset-9-gcc-c++ && \
    source /opt/rh/devtoolset-9/enable && \
    yum install -y make wget readline-devel unzip git && \
    mkdir /root/build && \
    cd /root/build && \
    wget https://github.com/LuaJIT/LuaJIT/archive/refs/tags/v2.0.5.zip --no-check-certificate && \
    unzip v2.0.5.zip && \
    cd LuaJIT-2.0.5 && \
    make && make install && \
    cd ../ && \
    wget https://luarocks.org/releases/luarocks-3.9.1.tar.gz --no-check-certificate && \
    cd luarocks-3.9.1 && \
    ./configure && make && make install && \
    cd ../ && \
    wget http://pyyaml.org/download/libyaml/yaml-0.2.5.tar.gz --no-check-certificate && \
    tar zxvf yaml-0.2.5.tar.gz && \
    cd yaml-0.2.5 && \
    ./configure && make && make install && \
    cd ../ && \
    luarocks install lua-cjson && \
    luarocks install luasocket && \
    luarocks install lyaml && \
    luarocks install lbase64 && \
    luarocks install lua-csnappy && \
    luarocks install lua-protobuf && \
    luarocks install luaposix && \
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/ && \
    git clone https://gitee.com/chuyansz/sysak.git && \
    cd sysak/source/tools/monitor/unity/beeQ/ && \
    make