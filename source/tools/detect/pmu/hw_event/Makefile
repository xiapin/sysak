EXTRA_LDFLAGS += -static
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
mods:= $(OBJECTS)

target := hw_event

include $(SRC)/mk/csrc.mk
