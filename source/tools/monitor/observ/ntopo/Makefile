TARGET_PATH := $(OBJ_TOOLS_ROOT)

.PHONY: ntopo

ntopo: bin target_rule

bin: 
	cargo build --release
	cp target/release/ntopo $(TARGET_PATH)/

target := ntopo

include $(SRC)/mk/target.inc