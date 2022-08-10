
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := irqoff
EXTRA_LDFLAGS += -lsysak -lrt
EXTRA_CFLAGS += -static

include $(SRC)/mk/bpf.mk
