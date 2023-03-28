CLANG ?= clang
LLVM_STRIP ?= llvm-strip
BPFTOOL ?= $(SRC)/lib/internal/ebpf/tools/bpftool
APPS_DIR := $(abspath .)
prefix ?= /usr/local
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/' )
COOLBPF_OBJ += $(OBJ_LIB_PATH)/libbpf.a $(OBJ_LIB_PATH)/coolbpf.a

ifeq ($(KERNEL_DEPEND), Y)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

DEPEND := "prev{btf}"

CFLAGS += $(EXTRA_CLFAGS) -g -O2 -Wall
LDFLAGS += $(EXTRA_LDFLAGS)
INCLUDES += $(EXTRA_INCLUDES) -I$(OBJPATH) -I$(SRC)/lib/internal/ebpf -I$(TARGET_PATH) -I$(OBJ_LIB_PATH) -I$(SRC)/lib/internal/ebpf/coolbpf/third/libbpf/include/uapi -I$(SRC)/lib/uapi/include

ifeq ($(V),1)
	Q =
	msg =
else
	Q = @
	msg = @printf '  %-8s %s%s\n'                                   \
		"$(1)"                                            \
		"$(patsubst $(abspath $(TARGET_PATH))/%,%,$(2))"       \
		"$(if $(3), $(3))";
	MAKEFLAGS += --no-print-directory
endif

newdirs := $(addprefix $(OBJPATH)/, $(newdirs))

cobjs := $(patsubst %.c, %.o, $(csrcs))
target_cobjs := $(foreach n, $(cobjs), $(OBJPATH)/$(n))

bpfobjs := $(patsubst %.c, %.o, $(bpfsrcs))
target_bpfobjs := $(foreach n, $(bpfobjs), $(OBJPATH)/$(n))

bpfskel := $(patsubst %.bpf.o, %.skel.h, $(target_bpfobjs))

all: $(target) target_rule

$(target): $(target_cobjs) $(bpfskel) $(COOLBPF_OBJ)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) $^ -lelf -lz -o $(TARGET_PATH)/$@ -L$(OBJ_LIB_PATH) $(LDFLAGS)
$(target_cobjs): $(cobjs)

$(cobjs): %.o : %.c $(bpfskel)
	$(call msg,CC,$@) 
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $(OBJPATH)/$@

$(bpfskel): %.skel.h : %.bpf.o $(target_bpfobjs)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

$(target_bpfobjs): $(bpfobjs)

NEWVER := $(shell echo $(KERNEL_VERSION)|awk -F. '{if($$1>3) print 1; else print 0}')
ifeq ($(NEWVER),1)
	KERN_VER := -DVER310_LATER
endif
$(bpfobjs) : %.o : %.c dirs
	$(call msg,BPF,$@)
	$(CLANG) -g -O2 $(KERN_VER) -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES) -c $< -o $(OBJPATH)/$@
	$(Q)$(LLVM_STRIP) -g $(OBJPATH)/$@ # strip useless DWARF info

dirs:
	mkdir -p $(newdirs)

# delete failed targets
.DELETE_ON_ERROR:

# keep intermediate (.skel.h, .bpf.o, etc) targets
# .SECONDARY:
include $(SRC)/mk/target.inc
