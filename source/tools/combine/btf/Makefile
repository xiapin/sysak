target := btf

ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

SRC_VMLINUX := vmlinux-btf/vmlinux-$(KERNEL_VERSION)

ifeq ($(SRC_VMLINUX), $(wildcard $(SRC_VMLINUX)))
$(target): target_rule
	cp btf.sh $(OBJ_TOOLS_ROOT)/btf
	cp $(SRC_VMLINUX) $(OBJ_TOOLS_PATH)/
	cp pahole $(OBJ_TOOLS_ROOT)/pahole
else
$(target): target_rule
	cp btf.sh $(OBJ_TOOLS_ROOT)/btf
	cp pahole $(OBJ_TOOLS_ROOT)/pahole
endif

include $(SRC)/mk/target.inc
#include $(SRC)/mk/sh.mk
