CLANG ?= clang
LLVM_STRIP ?= llvm-strip
BPFTOOL ?= $(SRC)/lib/internal/ebpf/tools/bpftool
prefix ?= /usr/local
ARCH := $(shell uname -m | sed 's/x86_64/x86/')
COOLBPF_OBJ := $(OBJ_LIB_PATH)/libbpf.a $(OBJ_LIB_PATH)/coolbpf.a
CXX ?= g++

# source/mk/target.inc use $(TARGET_PATH)
ifeq ($(KERNEL_DEPEND), Y)
OUTPUT := $(OBJ_TOOLS_PATH)
TARGET_PATH := $(OBJ_TOOLS_PATH)
else
OUTPUT := $(OBJ_TOOLS_ROOT)
TARGET_PATH := $(OBJ_TOOLS_ROOT)
endif

DEPEND := "prev{btf}"

CFLAGS += $(EXTRA_CLFAGS) -g -O2 -Wall
LDFLAGS += $(EXTRA_LDFLAGS) -lelf -lz
INCLUDES += $(EXTRA_INCLUDES) -I$(OBJPATH) -I$(SRC)/lib/internal/ebpf -I$(OUTPUT) -I$(OBJ_LIB_PATH) -I$(SRC)/lib/internal/ebpf/coolbpf/third/libbpf/include/uapi -I$(OBJPATH)/src

ifeq ($(V),1)
	Q =
	msg =
else
	Q = @
	msg = @printf '  %-8s %s%s\n'                                   \
		"$(1)"                                            \
		"$(patsubst $(abspath $(OUTPUT))/%,%,$(2))"       \
		"$(if $(3), $(3))";
	MAKEFLAGS += --no-print-directory
endif

newdirs := $(addprefix $(OBJPATH)/, $(newdirs))

cppobjs := $(patsubst %.cc, %.o, $(cppsrcs))
target_cppobjs := $(foreach n, $(cppobjs), $(OBJPATH)/$(n))

bpfobjs := $(patsubst %.c, %.o, $(bpfsrcs))
target_bpfobjs := $(foreach n, $(bpfobjs), $(OBJPATH)/$(n))
bpfskel := $(patsubst %.bpf.o, %.skel.h, $(target_bpfobjs))

all: $(target) target_rule

$(target): $(target_cppobjs) $(bpfskel) $(COOLBPF_OBJ)
	$(call msg,BINARY,$@)
	$(Q)$(CXX) $(CFLAGS) $(INCLUDES) $^ -o $(OUTPUT)/$@ $(LDFLAGS)

$(target_cppobjs) : $(cppobjs)

$(cppobjs): %.o : %.cc $(bpfskel)
	$(call msg,CXX,$@) 
	$(Q)$(CXX) $(CFLAGS) $(INCLUDES) -c $< -o $(OBJPATH)/$@

$(bpfskel): %.skel.h : %.bpf.o $(target_bpfobjs)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

$(target_bpfobjs): $(bpfobjs)

$(bpfobjs) : %.o : %.c dirs
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES) -c $< -o $(OBJPATH)/$@
	$(Q)$(LLVM_STRIP) -g $(OBJPATH)/$@ # strip useless DWARF info

dirs:
	mkdir -p $(newdirs)

# delete failed targets
.DELETE_ON_ERROR:

# keep intermediate (.skel.h, .bpf.o, etc) targets
# .SECONDARY:

include $(SRC)/mk/target.inc
