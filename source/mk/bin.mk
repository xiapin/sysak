ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
SOURCE_PATH := $(KERNEL_VERSION)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
SOURCE_PATH := .
endif

all: $(target) target_rule

exist := $(shell if [ -f $(SOURCE_PATH)/$(target) ]; then echo "exist"; else echo "notexist"; fi;)
ifeq ($(exist), exist)
$(target):
	cp $(SOURCE_PATH)/$(target) $(TARGET_PATH)/
else
$(target):
	@echo no kernel version
endif

include $(SRC)/mk/target.inc

.PHONY: $(target)
