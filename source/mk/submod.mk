ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

all: $(target) target_rule

$(target):
	make -C $(SUBMOD_SRC) $(MAKE_ARGS) INSTALL_PRE=$(TARGET_PATH) install

.PHONY: $(target)

include $(SRC)/mk/target.inc
