LDFLAGS += -l:liblog4cpp.a -lncurses -lz -pthread -ltinfo -ldl
CFLAGS += -std=c++11 -static
INCLUDES += -Isrc/ -Isrc/bpf -I$(SRC)/lib/internal/ebpf/libbpf/src

newdirs := $(shell find src/ -type d)
cppsrcs := $(shell find ./src -name "*.cc")
bpfsrcs := $(wildcard src/bpf_prog/*.bpf.c)
target := pingtrace

DEP_PKG_INSTALL_1 := "wget https://sourceforge.net/projects/log4cpp/files/log4cpp-1.1.x%20%28new%29/log4cpp-1.1/log4cpp-1.1.3.tar.gz --no-check-certificate@tar -xzf log4cpp-1.1.3.tar.gz -C ./@cd log4cpp@./configure@make@make install"
DEP_PKG_INSTALL_2 := "git clone https://github.com/Tencent/rapidjson.git@cd rapidjson@git submodule update --init@mkdir build@cd build@cmake ..@make install"
DEP_PKG_INSTALL_3 := "git clone https://github.com/CLIUtils/CLI11.git@cd CLI11@git checkout 34c4310d9907f6a6c2eb5322fa7472474800577c@git submodule update --init@mkdir build@cd build@cmake ..@make install"

include $(SRC)/mk/ccbpf.mk
