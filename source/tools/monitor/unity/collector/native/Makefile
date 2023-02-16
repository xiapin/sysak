CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := procffi.o sig_stop.o unity_interface.o
SO := libprocffi.so

all: $(SO)

%.o: %.c %.h
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f *.so $(OBJS)
