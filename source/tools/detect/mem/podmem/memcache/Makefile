target = memcache
LIBS += -l:coolbpf.a -lelf -lz -lpthread  -l:libbpf.a
LDFLAGS += $(LIBS) -Wall
mods := memread.o memcg.o offset.o
include $(SRC)/mk/cc.mk
