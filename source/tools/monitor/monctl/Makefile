target := monctl

mods := monctl.o

objs := $(foreach n, $(mods), $(OBJPATH)/$(n))

CFLAGS += $(EXTRA_CFLAGS) -I$(SRC)/lib/uapi/include
LDFLAGS += $(EXTRA_LDFLAGS)

ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

INSTALL_PRE=$(TARGET_PATH)

all: $(target) target_rule config

$(target): $(objs)
	gcc -o $(TARGET_PATH)/$@ $^ -L$(OBJ_LIB_PATH) $(LDFLAGS)

$(objs): $(mods)

$(mods): %.o : %.c
	gcc -I. $(CFLAGS) -c -o $(OBJPATH)/$@ $<

config:
	#mkdir for monctl
	mkdir -p $(INSTALL_PRE)/monitor/
	#copy config file
	cp conf/monctl.conf $(INSTALL_PRE)/monitor/

uninstall:
	#rm conf file
	rm -rf $(INSTALL_PRE)/monitor/

include $(SRC)/mk/target.inc
