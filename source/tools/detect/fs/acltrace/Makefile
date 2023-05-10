
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := acltrace
EXTRA_LDFLAGS += -lsysak

include $(SRC)/mk/bpf.mk
