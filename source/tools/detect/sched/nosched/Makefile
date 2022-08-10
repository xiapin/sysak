
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := nosched
EXTRA_LDFLAGS += -lrt -lsysak

include $(SRC)/mk/bpf.mk
