#!/bin/bash

if [ "$1" == "clean" ]; then
	echo "start clean...."
	rm -rf dist/*
else
echo "start complie...."
cd cmd/ebpf_collector/
go build
cd ../../
mv cmd/ebpf_collector/ebpf_collector dist/
cp ebpf/src/libnet.so dist/
cp config.yaml dist/

fi
