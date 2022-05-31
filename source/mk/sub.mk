exclude_dirs := include inc
dirs := $(shell find . -maxdepth 1 -type d)
dirs := $(basename $(patsubst ./%,%,$(dirs)))
dirs:=$(filter-out $(exclude_dirs),$(dirs))

SUBDIRS := $(dirs)

.PHONY: subdirs $(SUBDIRS) clean

all: subdirs target_rule

subdirs: $(SUBDIRS)
$(SUBDIRS):
	make -C $@

ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

include $(SRC)/mk/target.inc
