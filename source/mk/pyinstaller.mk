ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

all: $(target) target_rule

$(target): $@
	rm -Rf build dist
	pyinstaller --clean $@.spec
	pyinstaller $@.spec
	cp dist/$@ $(TARGET_PATH)/

include $(SRC)/mk/target.inc
