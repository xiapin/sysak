
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := irqoff
EXTRA_LDFLAGS += -lsysak

include $(SRC)/mk/bpf.mk
