ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

.PHONY: $(mods)

all: $(target) target_rule

$(target): $(mods)
	cp $@.sh $(TARGET_PATH)/$@

$(mods):
	cp $@ $(TARGET_PATH)/ -rf

include $(SRC)/mk/target.inc
