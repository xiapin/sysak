target = memcache
LIBS += libbpf.a -lelf -lz
LDFLAGS += -Wall -static-libstdc++ -L./ $(LIBS)
mods := memread.o memcg.o offset.o
include $(SRC)/mk/cc.mk
