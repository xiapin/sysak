# SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
OUTPUT := .output
CLANG ?= clang
LLVM_STRIP ?= llvm-strip
BPFTOOL ?= $(abspath ./tools/bpftool)
LIBBPF_SRC := $(abspath ./libbpf/src)
LIBBPF_OBJ := $(abspath $(OUTPUT)/libbpf.a)
VMLINUX := vmlinux/vmlinux.h
LIBBPF_VERSION ?= v0.8.1
# Use our own libbpf API headers and Linux UAPI headers distributed with
# libbpf to avoid dependency on system-wide headers, which could be missing or
# outdated
INCLUDES := -I$(OUTPUT) -I../../libbpf/include/uapi -I$(dir $(VMLINUX))
CFLAGS := -g -Wall -fPIC
MYCFLAGS := -ggdb -gdwarf -O2 -Wall -fpie -Wno-unused-variable -Wno-unused-function
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/' | sed 's/ppc64le/powerpc/' | sed 's/mips.*/mips/')

APPS = net nginx

# Get Clang's default includes on this system. We'll explicitly add these dirs
# to the includes list when compiling with `-target bpf` because otherwise some
# architecture-specific dirs will be "missing" on some architectures/distros -
# headers such as asm/types.h, asm/byteorder.h, asm/socket.h, asm/sockios.h,
# sys/cdefs.h etc. might be missing.
#
# Use '-idirafter': Don't interfere with include mechanics except where the
# build would have failed anyways.
CLANG_BPF_SYS_INCLUDES = $(shell $(CLANG) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }')

ifeq ($(V),1)
	Q =
	msg =
else
	Q = @
	msg = @printf '  %-8s %s%s\n'					\
		      "$(1)"						\
		      "$(patsubst $(abspath $(OUTPUT))/%,%,$(2))"	\
		      "$(if $(3), $(3))";
	MAKEFLAGS += --no-print-directory
endif

.PHONY: all
all: $(APPS) libnginx.a libnet.a profile

.PHONY: clean
clean:
	$(call msg,CLEAN)
	$(Q)rm -rf $(OUTPUT) lib*.so lib*.a *.o
	rm -rf bpfsrc

.PHONY: bpf
bpf: $(OUTPUT)/%.bpf.o | $(OUTPUT)

COMM_OBJ = \
	   $(OUTPUT)/uprobe_helpers.o \

$(OUTPUT) $(OUTPUT)/libbpf:
	$(call msg,MKDIR,$@)
	$(Q)mkdir -p $@

# Build libbpf
$(LIBBPF_OBJ): $(wildcard $(LIBBPF_SRC)/*.[ch] $(LIBBPF_SRC)/Makefile) | $(OUTPUT)/libbpf
	$(call msg,LIB,$@)
	$(Q)$(MAKE) -C $(LIBBPF_SRC) BUILD_STATIC_ONLY=1		      \
		    OBJDIR=$(dir $@)/libbpf DESTDIR=$(dir $@)		      \
		    INCLUDEDIR= LIBDIR= UAPIDIR=			      \
		    install

# Build BPF code
$(OUTPUT)/%.bpf.o: %.bpf.c $(LIBBPF_OBJ) $(wildcard %.h) $(VMLINUX) | $(OUTPUT)
	$(call msg,BPF,$@)
	$(Q)$(CLANG) -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) $(INCLUDES) $(CLANG_BPF_SYS_INCLUDES) -c $(filter %.c,$^) -o $@
	$(Q)$(LLVM_STRIP) -g $@ # strip useless DWARF info

# Generate BPF skeletons
$(OUTPUT)/%.skel.h: $(OUTPUT)/%.bpf.o | $(OUTPUT)
	$(call msg,GEN-SKEL,$@)
	$(Q)$(BPFTOOL) gen skeleton $< > $@

# Build user-space code
$(patsubst %,$(OUTPUT)/%.o,$(APPS)): %.o: %.skel.h

$(OUTPUT)/%.o: %.c $(wildcard %.h) | $(OUTPUT)
	$(call msg,CC,$@)
	$(Q)$(CC) $(CFLAGS) $(INCLUDES) -static -c $(filter %.c,$^) -o $@

# Build application binary so
$(APPS):%: $(OUTPUT)/%.o $(LIBBPF_OBJ) $(COMM_OBJ) | $(OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(CC) $(CFLAGS) -shared -fPIC $^ lib/libelf_pic.a -o lib$@.so

# Build app binary static lib
libnet.a: $(OUTPUT)/net.o $(LIBBPF_OBJ) $(COMM_OBJ) | $(OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(AR) rcs $@ $^

libnginx.a: $(OUTPUT)/nginx.o $(LIBBPF_OBJ) $(COMM_OBJ) | $(OUTPUT)
	$(call msg,BINARY,$@)
	$(Q)$(AR) rcs $@ $^

.PHONY: build_libbpf
build_libbpf:
	test -d bpfsrc || git clone https://github.com/libbpf/libbpf bpfsrc
	cd bpfsrc && git checkout $(LIBBPF_VERSION)
	PREFIX=$(shell pwd)/lib make -C bpfsrc/src -j16 install

profile: oncpu.bpf.o offcpu.bpf.o user_slow.bpf.o ping_slow.bpf.o nginx.bpf.o

oncpu.bpf.o: oncpu.bpf.c profile.bpf.h $(VMLINUX)
	$(CLANG) $(MYCFLAGS) -target bpf -D__TARGET_ARCH_x86 -I. -c oncpu.bpf.c -o $@

offcpu.bpf.o: offcpu.bpf.c $(VMLINUX)
	$(CLANG) $(MYCFLAGS) -target bpf -D__TARGET_ARCH_x86 -I. -c offcpu.bpf.c -o $@

user_slow.bpf.o: user_slow.bpf.c $(VMLINUX)
	$(CLANG) $(MYCFLAGS) -target bpf -D__TARGET_ARCH_x86 -I. -c user_slow.bpf.c -o $@

ping_slow.bpf.o: ping_slow.bpf.c $(VMLINUX)
	$(CLANG) $(MYCFLAGS) -target bpf -D__TARGET_ARCH_x86 -I. -c ping_slow.bpf.c -o $@

nginx.bpf.o: nginx.bpf.c $(VMLINUX)
	$(CLANG) $(MYCFLAGS) -target bpf -D__TARGET_ARCH_x86 -I. -c nginx.bpf.c -o $@


# delete failed targets
.DELETE_ON_ERROR:

# keep intermediate (.skel.h, .bpf.o, etc) targets
.SECONDARY:

