target := memcgoffline
LIBS += -L ${OBJ_LIB_PATH}/lib -l:libcoolbpf.a -l:libsysak.a -lelf -lz
INCLUDES += -I$(SRC)/tools/detect/mem/memcgoffline/include
LDFLAGS += -Wall $(LIBS)
mods := memcg_iter.o memcgoffline.o

include $(SRC)/mk/csrc.mk