target := pingEdge
ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

all: $(target) target_rule

$(target): $@
	sh venv.sh
	cp dist/pingEdge $(TARGET_PATH)/

include $(SRC)/mk/target.inc