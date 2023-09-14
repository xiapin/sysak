FROM registry.cn-hangzhou.aliyuncs.com/sysom/lcc
MAINTAINER "liaozhaoyan <zhaoyan.lzy@alibaba-inc.com>"
WORKDIR /root/
RUN source /opt/rh/devtoolset-9/enable && \
    yum install -y make wget lua-devel unzip git numactl-devel m4 && \
    yum install -y golang && \
    mkdir /root/build && \
    cd /root/build && \
    git clone https://gitee.com/chuyansz/sysak.git && \
    cd sysak/source/tools/monitor/unity/third/ && \
    unzip v2.0.5.zip && \
    cd LuaJIT-2.0.5 && \
    make && make install && \
    cd ../ && \
    tar zxvf luarocks-3.9.1.tar.gz && \
    cd luarocks-3.9.1 && \
    ./configure && make && make install && \
    cd ../ && \
    tar zxvf yaml-0.2.5.tar.gz && \
    cd yaml-0.2.5 && \
    ./configure && make && make install && \
    cd ../ && \
    tar curl-7.87.0.tar.gz && \
    cd curl-7.87.0 && \
    ./configure --with-openssl && make && make install && \
    cd ../ && \
    luarocks install luasec && \
    luarocks install lua-cjson && \
    luarocks install luasocket && \
    luarocks install lyaml && \
    luarocks install lbase64 && \
    luarocks install lua-csnappy && \
    luarocks install lua-protobuf && \
    luarocks install sha1 && \
    luarocks install md5 && \
    luarocks install luaposix 35.1-1 && \
    luarocks install http && \
    luarocks install inotify && \
    luarocks install luafilesystem && \
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/ && \
    cd ../beeQ/ && \
    make