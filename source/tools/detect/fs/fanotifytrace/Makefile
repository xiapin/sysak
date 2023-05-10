
newdirs := $(shell find ./ -type d)

bpfsrcs := $(wildcard bpf/*.bpf.c)
csrcs := $(wildcard *.c)
target := fanotifytrace

include $(SRC)/mk/bpf.mk
