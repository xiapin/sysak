
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := getdelays
EXTRA_LDFLAGS += -lsysak

include $(SRC)/mk/bpf.mk
