EXTRA_LDFLAGS += -static
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
mods:= $(OBJECTS)

target := cmtcat

include $(SRC)/mk/csrc.mk
