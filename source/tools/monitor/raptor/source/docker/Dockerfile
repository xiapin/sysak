FROM golang:1.19-alpine3.16

RUN apk update && apk upgrade && \
    apk add cmake --no-cache binutils clang make git zstd gcc g++ libc-dev musl-dev bash zlib-dev elfutils-dev libelf-static zlib-static openssh llvm \
    linux-headers
