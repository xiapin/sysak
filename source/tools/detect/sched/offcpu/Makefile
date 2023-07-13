
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := offcputime
EXTRA_LDFLAGS += -lrt -lsysak
EXTRA_CFLAGS += -static

include $(SRC)/mk/bpf.mk
