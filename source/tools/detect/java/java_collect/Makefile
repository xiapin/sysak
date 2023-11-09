target := java_collect
ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

all: $(target) target_rule

$(target): $@
	cp -r continuous-profile-collector $(TARGET_PATH)/
	mv $(TARGET_PATH)/continuous-profile-collector $(TARGET_PATH)/jruntime
	cd src && pwd && sh venv.sh && cd ../
	cp src/dist/java_collect $(TARGET_PATH)/

include $(SRC)/mk/target.inc
