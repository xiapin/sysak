
target := unity

all:
	cp $(OBJ_LIB_PATH)/lib/libcoolbpf.so* collector/native/

	echo "build for beeQ"
	make dist -C beeQ
clean:
	make -C collector/plugin clean
