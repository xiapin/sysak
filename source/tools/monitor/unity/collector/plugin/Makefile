CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := proto_sender.o
LIB := libproto_sender.a


DEPMOD=sample threads kmsg proc_schedstat proc_loadavg bpfsample2 unity_nosched unity_irqoff cpudist net_health net_retrans netlink numainfo cpufreq gpuinfo


all: $(LIB) $(DEPMOD)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(LIB): $(OBJS)
	$(AR) cr $@ $(OBJS)

$(DEPMOD):ECHO
	make -C $@

ECHO:
	@echo $(DEPMOD)

clean:
	$(foreach N, $(DEPMOD),make clean -C $(N);)
	rm -rf *.o *.a
