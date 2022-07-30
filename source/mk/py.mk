ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

all: $(target) target_rule

$(target): $(pymods)
	cp $@.py $(TARGET_PATH)/$@

$(pymods): %: %.py
	cp $< $(TARGET_PATH)/$@.py

include $(SRC)/mk/target.inc
