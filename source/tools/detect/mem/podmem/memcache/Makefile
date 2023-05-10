target = memcache
LIBS += -L ${OBJ_LIB_PATH}/lib -l:libcoolbpf.a -lelf -lz -lpthread
LDFLAGS +=  -Wall -static-libstdc++ $(LIBS)
mods := memread.o memcg.o offset.o
include $(SRC)/mk/cc.mk
