FROM registry.cn-hangzhou.aliyuncs.com/sysom/lcc
MAINTAINER "liaozhaoyan <zhaoyan.lzy@alibaba-inc.com>"
WORKDIR /root/
RUN source /opt/rh/devtoolset-9/enable && \
    yum install -y make wget  * unzip git && \
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
    luarocks install luasec && \
    luarocks install lua-cjson && \
    luarocks install luasocket && \
    luarocks install lyaml && \
    luarocks install lbase64 && \
    luarocks install lua-csnappy && \
    luarocks install lua-protobuf && \
    luarocks install luaposix && \
    luarocks install sha1 && \
    luarocks install md5 && \
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/ && \
    cd ../beeQ/ && \
    make