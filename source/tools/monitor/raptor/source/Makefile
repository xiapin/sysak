GIT_COMIID := $(shell git rev-parse --short HEAD)
GIT_TAG := $(shell git describe --tags --abbrev=0)
GIT_BRANCH := $(shell git rev-parse --abbrev-ref HEAD)

RELEASE_VERSION := $(GIT_BRANCH)_$(GIT_COMIID)
RELEASE_TIME := $(shell date -u '+%Y-%m-%dT%H:%M:%SZ')
RELEASE_COMMIT := $(GIT_COMIID)
RELEASE_GOVERSION := $(shell go version)
RELEASE_AUTHOR := Dylane
GOLDFLAGS :=

EXTRA_CGO_CFLAGS := -I$(abspath ./ebpf/src/libbpf/lib/include) \
	-I$(abspath ./ebpf/src/bcc/lib/include/bcc_syms)
EXTRA_CGO_LDFLAGS := -L$(abspath ./ebpf/src/libbpf/lib/lib64) -lbpf \
		-L$(abspath ./ebpf/src/bcc/lib/lib) -lbcc-syms -lstdc++ -lelf -lz \
		-L$(abspath ./ebpf/src/lib) -lnginx

.PHONY: goenv
goenv:
	@echo "Config Golang Env..."
	bash ./config_goenv.sh

.PHONY: build
build:
	@echo "Complie Golong Code..." \
	CGO_CFLAGS="$(EXTRA_CGO_CFLAGS)" \
	CGO_LDFLAGS="$(EXTRA_CGO_LDFLAGS)" \
	go build -ldflags "-linkmode external -extldflags '-static' -X 'main.version=$(RELEASE_VERSION)' \
	-X 'main.commitId=$(RELEASE_COMMIT)' -X 'main.releaseTime=$(RELEASE_TIME)' \
	-X 'main.goVersion=$(RELEASE_GOVERSION)' -X 'main.author=$(RELEASE_AUTHOR)'" -o raptor

.PHONY: ebpf
ebpf:
	@echo "COMPLIE eBPF ALL..."
	$(MAKE) -C ebpf/src
	# hard code
	cp -f ebpf/src/libnginx.a nginx/
	cp -f ebpf/src/*.o  profile/agent/ebpf/bpf
	cp -f ebpf/src/nginx.bpf.o  nginx

.PHONY: profile
profile:
	@echo "COMPLIE eBPF PROFILE..."
	$(MAKE) -C ebpf/src profile
	cp -f ebpf/src/*.o  profile/agent/ebpf/bpf
	cp -f ebpf/src/nginx.bpf.o  nginx

.PHONY: clean
clean:
	rm -rf raptor
	rm -rf profile/agent/ebpf/bpf/*.o nginx/*.o
	rm -rf nginx/libnginx.a
	$(MAKE) -C ebpf/src clean

install:profile
	make
	cp raptor $(INSTALL_PRE)/
