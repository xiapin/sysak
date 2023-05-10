CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := procffi.o ffi_unity_api.o
SO := libprocffi.so

all: $(SO)

%.o: %.c %.h
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f *.so $(OBJS)
