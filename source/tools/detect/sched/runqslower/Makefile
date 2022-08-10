
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := runqslower
EXTRA_LDFLAGS += -lrt -lsysak
EXTRA_CFLAGS += -static

include $(SRC)/mk/bpf.mk
