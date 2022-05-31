
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := runqslower
EXTRA_LDFLAGS += -lsysak

include $(SRC)/mk/bpf.mk
