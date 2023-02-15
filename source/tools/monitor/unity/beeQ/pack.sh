#!/bin/bash
DIST=dist
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

mkdir ${DIST}/lib
cp -r /usr/local/lib/lua/5.1/* ${DIST}/lib/

mkdir ${DIST}/lua
cp -r /usr/local/share/lua/5.1/* ${DIST}/lua/

mkdir ${APP}
mkdir ${APP}/beaver
cp -r beaver/guide ${APP}/beaver/
cp beaver/*.lua ${APP}/beaver/

mkdir ${APP}/beeQ/
mkdir ${APP}/beeQ/lib
cp beeQ/lib/*.so* ${APP}/beeQ/lib/
cp beeQ/*.lua ${APP}/beeQ/
cp beeQ/unity-mon ${APP}/beeQ/
cp beeQ/run.sh ${APP}/beeQ/

mkdir ${APP}/collector
mkdir ${APP}/collector/native
cp collector/native/*.so* ${APP}/collector/native/
cp collector/native/*.lua ${APP}/collector/native/
cp collector/*.lua ${APP}/collector/
cp collector/plugin.yaml ${APP}/collector/

mkdir ${APP}/common
cp common/*.lua ${APP}/common/

mkdir ${APP}/httplib
cp httplib/*.lua ${APP}/httplib/

mkdir ${APP}/tsdb
mkdir ${APP}/tsdb/native
cp tsdb/native/*.so* ${APP}/tsdb/native/
cp tsdb/native/*.lua ${APP}/tsdb/native/
cp tsdb/*.lua ${APP}/tsdb

tar zcv -f dist.tar.gz dist/
