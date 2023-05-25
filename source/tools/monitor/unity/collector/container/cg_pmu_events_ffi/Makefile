CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := cg_pmu_ffi.o
SO := libcgpmuffi.so

DEPMOD= ../collector
all: $(SO) install

%.o: %.c %.h
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../native/
clean:
	rm -f *.so $(OBJS)
