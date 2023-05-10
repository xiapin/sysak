
target := unity

all:
	cp $(OBJ_LIB_PATH)/libbpf.so collector/native/
	cp $(OBJ_LIB_PATH)/libbpf.so.0 collector/native/
	cp $(OBJ_LIB_PATH)/libbpf.so.0.6.0 collector/native/

	cp $(OBJ_LIB_PATH)/libcoolbpf.so collector/native/
	cp $(OBJ_LIB_PATH)/libcoolbpf.so.0 collector/native/
	cp $(OBJ_LIB_PATH)/libcoolbpf.so.0.1.0 collector/native/
	make dist -C beeQ
