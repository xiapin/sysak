ifneq ($(wildcard config-host.mak),)
include config-host.mak
else
config-host.mak:
	@echo "Please call configure before running make!"
	@exit 1
endif

SRC := $(shell pwd)/source

OBJ_LIB_PATH := $(OBJPATH)/.sysak_components/lib/$(KERNEL_VERSION)
OBJ_TOOLS_ROOT := $(OBJPATH)/.sysak_components/tools
OBJ_TOOLS_PATH := $(OBJPATH)/.sysak_components/tools/$(KERNEL_VERSION)
SYSAK_RULES := .sysak.rules

export KERNEL_VERSION
export SRC
export OBJPATH
export OBJ_LIB_PATH
export OBJ_TOOLS_ROOT
export OBJ_TOOLS_PATH
export SYSAK_RULES
export BUILD_KERNEL_MODULE
export BUILD_LIBBPF

export EXTRA_LDFLAGS
export TARGET_LIST

.PHONY: all lib tools binary install $(TARGET_LIST)
all: config-host.mak $(OBJ_LIB_PATH) $(OBJ_TOOLS_PATH) clean_rules lib tools binary

clean_rules:
	find $(OBJPATH)/.sysak_components -name ".sysak.rules" |xargs rm -rf

lib:
	make -C $(SRC)/lib

tools: $(TARGET_LIST)
$(TARGET_LIST):
	make -C $@ -j

binary:
	$(CC) -o $(SRC)/sysak $(SRC)/sysak.c
	cp $(SRC)/sysak $(OBJPATH)/
	chmod +x $(OBJPATH)/sysak
	chmod +x $(OBJPATH)/.sysak_components/tools/* -R

.PHONY: clean clean_middle dist_clean
clean:
	make -C $(SRC)/lib clean
	make -C $(SRC)/tools/monitor/mservice/master clean
	rm -rf $(OBJPATH)
clean_middle:
	make -C $(SRC)/lib clean
	rm -rf $(OBJPATH)/*.o
dist_clean: clean
	rm -rf config-host.mak
	rm -rf source/lib/internal/kernel_module
	rm -rf source/lib/internal/ebpf/libbpf

$(OBJ_LIB_PATH):
	mkdir -p $(OBJ_LIB_PATH)
$(OBJ_TOOLS_PATH):
	mkdir -p $(OBJ_TOOLS_PATH)

install:
	mkdir -p /usr/local/sysak/
	mkdir -p /usr/bin/
	cp $(OBJPATH)/sysak /usr/bin/sysak
	cp $(OBJPATH)/.sysak_components /usr/local/sysak/ -rf
	mkdir -p /etc/sysak
	mkdir -p /var/log/sysak
ifneq ($(wildcard $(OBJPATH)/.sysak_components/tools/monitor/sysakmon.conf),)
		cp $(OBJPATH)/.sysak_components/tools/monitor/sysakmon.conf /etc/sysak/
		cp $(SRC)/../rpm/sysak.service /usr/lib/systemd/system/
endif

uninstall:
	rm -rf /etc/sysak
	rm -rf /usr/bin/sysak
	rm -rf /usr/local/sysak
