
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := kvmexittime
EXTRA_LDFLAGS += -lsysak

include $(SRC)/mk/bpf.mk