
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := process_tree
EXTRA_LDFLAGS += -lrt -lsysak -lpthread
EXTRA_CFLAGS += -g
INCLUDES += -I$(SRC)/lib/internal/ebpf/coolbpf/third/libbpf/src

include $(SRC)/mk/bpf.mk
