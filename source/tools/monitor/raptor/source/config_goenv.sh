#!/bin/bash

GOVERSION=$(go version | awk '{print $3}')

if [ "$GOVERSION" = "go.1.19.3" ]; then
	echo "Golang Env Configed"
else
	echo "No Golang Env, We Start To Config"
	ARCH=$(uname -r | awk -F '.' '{print $NF}')
	if [ "$ARCH" = "x86_64" ]; then
		wget https://go.dev/dl/go1.19.3.linux-amd64.tar.gz
		tar -zxf go1.19.3.linux-amd64.tar.gz -C /usr/local/
	elif [ "$ARCH" = "aarch64" ]; then
		wget https://go.dev/dl/go1.19.3.linux-arm64.tar.gz
		tar -zxf go1.19.3.linux-arm64.tar.gz -C /usr/local/
	else
		echo "ARCH Not Support"
	fi

	sed -i '$a export GOROOT=/usr/local/go/' /etc/profile
	sed -i '$a\export GOPATH=/root/sdk/go' /etc/profile
	sed -i '$a\export PATH=$PATH:$GOROOT/bin:$GOPATH/bin' /etc/profile

	source /etc/profile

	go env -w GOPROXY="https://proxy.golang.com.cn,direct"
	echo "Golang Compile Env Config Success!"
fi
