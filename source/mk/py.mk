ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

all: $(target) target_rule

$(target): $(mods)
	cp $@.py $(TARGET_PATH)/$@

$(mods): %: %.py
	cp $< $(TARGET_PATH)/$@

include $(SRC)/mk/target.inc
