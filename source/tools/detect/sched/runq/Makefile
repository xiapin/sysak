
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := runqslower
EXTRA_LDFLAGS += -lrt -lsysak -lpthread

include $(SRC)/mk/bpf.mk
