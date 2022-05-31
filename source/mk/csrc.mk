objs := $(foreach n, $(mods), $(OBJPATH)/$(n))

CFLAGS += $(EXTRA_CFLAGS) -I$(SRC)/lib/uapi/include
LDFLAGS += $(EXTRA_LDFLAGS)

ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

all: $(target) target_rule

$(target): $(objs)
	gcc -o $(TARGET_PATH)/$@ $^ -L$(OBJ_LIB_PATH) $(LDFLAGS)

$(objs): $(mods)

$(mods): %.o : %.c
	gcc -I. $(CFLAGS) -c -o $(OBJPATH)/$@ $<

include $(SRC)/mk/target.inc
