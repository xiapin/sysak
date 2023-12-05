TARGET_PATH := $(OBJ_TOOLS_ROOT)

.PHONY: xwatcher

ntopo: bin target_rule

bin: 
	cargo build --release
	cp target/release/xwatcher $(TARGET_PATH)/

target := xwatcher

include $(SRC)/mk/target.inc