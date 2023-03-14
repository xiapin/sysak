LIBBPF_VERSION ?= v0.8.1

.PHONY: libbpf
libbpf:
	test -d src || git clone http://github.com/libbpf/libbpf src
	#test -d src || git clone git@github.com:libbpf/libbpf.git src
	cd src && git checkout $(LIBBPF_VERSION)
	#fix the low gcc version complie error
	$(shell sed -i 's/int i, err;/int i, err = 0;/g' src/src/btf_dump.c)
	PREFIX=$(shell pwd)/lib make -C src/src -j16 install

.PHONY: clean
clean:
	rm -rf src lib
