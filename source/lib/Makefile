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
	OBJDIR=$(OBJ_LIB_PATH)/coolbpf DESTDIR=$(OBJ_LIB_PATH) sh internal/ebpf/coolbpf/build.sh
endif

clean:
ifeq ($(BUILD_KERNEL_MODULE), YES)
	make -C internal/kernel_module clean
endif
ifeq ($(BUILD_LIBBPF), YES)
	make -C internal/ebpf/libbpf/src clean
endif

