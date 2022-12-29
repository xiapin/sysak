
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := workqlatency
EXTRA_LDFLAGS += -lsysak -lpthread

include $(SRC)/mk/bpf.mk
