ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

all: $(target) target_rule
$(target): 
	go env -w GOPROXY="https://proxy.golang.com.cn,direct" 
	go mod tidy
	go build  -o $(TARGET_PATH)/$@ $^

include $(SRC)/mk/target.inc