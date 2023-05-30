.PHONY: all clean

all:
ifeq ($(BUILD_KERNEL_MODULE), YES)
	make -C internal/kernel_module
	cp internal/kernel_module/sysak.ko $(OBJ_LIB_PATH)/
else
	sysak_module_tag=`curl https://gitee.com/anolis/sysak-module/tree/master | grep "anolis/sysak-module/commit/\w\{7\}" -o | awk -F "/" '{print $$4}'`; \
	echo sysak_module_tag=$$sysak_module_tag >> $(OBJ_LIB_PATH)/../$(SYSAK_RULES)
endif
	make -C uapi
ifeq ($(BUILD_LIBBPF), YES)
	cd internal/ebpf/coolbpf && mkdir -p build && cd build && cmake -DCMAKE_INSTALL_PREFIX=${OBJ_LIB_PATH} .. && make install
endif

clean:
ifeq ($(BUILD_KERNEL_MODULE), YES)
ifneq ($(wildcard internal/kernel_module/Makefile),)
	make -C internal/kernel_module clean
endif
endif
ifeq ($(BUILD_LIBBPF), YES)
ifneq ($(wildcard internal/ebpf/coolbpf/third/libbpf/src/Makefile),)
	make -C internal/ebpf/coolbpf/third/libbpf/src/ clean
endif
endif
