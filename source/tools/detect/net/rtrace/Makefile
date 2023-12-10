TARGET_PATH := $(OBJ_TOOLS_ROOT)

.PHONY: rtrace

rtrace: bin target_rule

bin: 
	cargo build --release --features embed-rtrace
	cp target/release/rtrace $(TARGET_PATH)/

target := rtrace

include $(SRC)/mk/target.inc