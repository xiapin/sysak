#!/bin/bash
DIST=$1/dist
APP=${DIST}/app

echo $DIST
echo $APP
cd ../
rm -rf $DIST
mkdir $DIST

mkdir ${DIST}/install
cp /usr/local/lib/libyaml-0.so* ${DIST}/install/
cp /usr/local/lib/libluajit-5.1.so* ${DIST}/install/
cp /usr/local/lib/libyaml.so* ${DIST}/install/
cp /usr/lib64/libssl.so* ${DIST}/install/
cp /usr/lib64/libcrypto.so* ${DIST}/install/
cp /usr/lib64/libgssapi_krb5.so* ${DIST}/install/
cp /usr/lib64/libkrb5.so* ${DIST}/install/
cp /usr/lib64/libcom_err.so* ${DIST}/install/
cp /usr/lib64/libk5crypto.so*  ${DIST}/install/
cp /usr/lib64/libkrb5support.so* ${DIST}/install/
cp /usr/lib64/libkeyutils.so* ${DIST}/install/
cp /usr/lib64/libresolv.so* ${DIST}/install/
cp /usr/lib64/libpcre.so* ${DIST}/install/

mkdir ${DIST}/lib
cp -r /usr/local/lib/lua/5.1/* ${DIST}/lib/

mkdir ${DIST}/lua
cp -r /usr/local/share/lua/5.1/* ${DIST}/lua/

mkdir ${APP}
mkdir ${APP}/beaver
mkdir ${APP}/beaver/native
mkdir ${APP}/beaver/query
cp -r beaver/guide ${APP}/beaver/
cp -r beaver/query ${APP}/beaver/
cp beaver/*.lua ${APP}/beaver/
cp beaver/native/*.lua ${APP}/beaver/native

mkdir ${APP}/beeQ/
mkdir ${APP}/beeQ/lib
mkdir ${APP}/beeQ/postQue
mkdir ${APP}/beeQ/rbtree
cp beeQ/lib/*.so* ${APP}/beeQ/lib/
cp beeQ/*.lua ${APP}/beeQ/
cp beeQ/postQue/*.lua ${APP}/beeQ/postQue/
cp beeQ/rbtree/*.lua ${APP}/beeQ/rbtree/
cp beeQ/unity-mon ${APP}/beeQ/
cp beeQ/run.sh ${APP}/beeQ/

# for collector
mkdir ${APP}/collector
mkdir ${APP}/collector/native
mkdir ${APP}/collector/guard
mkdir ${APP}/collector/outline
mkdir ${APP}/collector/postPlugin
mkdir ${APP}/collector/postEngine
mkdir ${APP}/collector/execEngine
mkdir ${APP}/collector/podMan
mkdir ${APP}/collector/container
mkdir ${APP}/collector/podMan/runtime
mkdir ${APP}/collector/io
mkdir ${APP}/collector/rdt
mkdir ${APP}/collector/rdt/plugin
mkdir ${APP}/collector/observe
mkdir ${APP}/collector/perfRun
cp collector/native/*.so* ${APP}/collector/native/
cp collector/native/*.lua ${APP}/collector/native/
cp collector/*.lua ${APP}/collector/
cp collector/guard/*.lua ${APP}/collector/guard
cp collector/outline/*.lua ${APP}/collector/outline
cp collector/postPlugin/*.lua ${APP}/collector/postPlugin
cp collector/postEngine/*.lua ${APP}/collector/postEngine
cp collector/execEngine/*.lua ${APP}/collector/execEngine
cp collector/container/*.lua ${APP}/collector/container/
cp collector/postPlugin/*.lua ${APP}/collector/postPlugin
cp collector/podMan/*.lua ${APP}/collector/podMan
cp collector/podMan/runtime/*.lua ${APP}/collector/podMan/runtime
cp collector/io/*.lua ${APP}/collector/io
cp collector/rdt/*.lua ${APP}/collector/rdt
cp collector/rdt/plugin/*.lua ${APP}/collector/rdt/plugin
cp collector/observe/*.lua ${APP}/collector/observe
cp collector/plugin.yaml ${APP}/collector/
cp collector/perfRun/perfRun.sh ${APP}/collector/perfRun/perfRun.sh


mkdir ${APP}/common
mkdir ${APP}/common/protobuf
mkdir ${APP}/common/protobuf/metricstore
cp common/*.lua ${APP}/common/
cp common/protobuf/metricstore/*.lua ${APP}/common/protobuf/metricstore/

mkdir ${APP}/httplib
cp httplib/*.lua ${APP}/httplib/

mkdir ${APP}/tsdb
mkdir ${APP}/tsdb/native
cp tsdb/native/*.so* ${APP}/tsdb/native/
cp tsdb/native/*.lua ${APP}/tsdb/native/
cp tsdb/*.lua ${APP}/tsdb
cp /usr/local/lib/lua/5.1/* -R ${DIST}/lib/
#tar zcv -f dist.tar.gz $DIST/

mkdir ${APP}/etc
cp etc/* ${APP}/etc/
