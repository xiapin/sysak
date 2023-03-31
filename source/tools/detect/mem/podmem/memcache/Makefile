target = memcache
LIBS += -l:coolbpf.a -lelf -lz -lpthread  -l:libbpf.a
LDFLAGS +=  -Wall -static-libstdc++ $(LIBS)
mods := memread.o memcg.o offset.o
include $(SRC)/mk/cc.mk
